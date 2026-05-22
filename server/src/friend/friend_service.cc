#include "friend/friend_service.h"

#include <algorithm>
#include <vector>

#include "common/proto_mapper.h"
#include "common/uuid.h"
#include "notify.pb.h"

namespace zchat {
namespace {

template <typename Response>
Response ErrorResponse(const std::string &request_id,
                       const std::string &message) {
    Response response;
    response.set_request_id(request_id);
    response.set_success(false);
    response.set_errmsg(message);
    return response;
}

template <typename Response>
void MarkOk(const std::string &request_id, Response *response) {
    response->set_request_id(request_id);
    response->set_success(true);
    response->set_errmsg("");
}

} // namespace

FriendApplicationService::FriendApplicationService(FriendRepository &friends,
                                                   UserRepository &users,
                                                   FileRepository &files,
                                                   MessageRepository &messages,
                                                   SessionStore &sessions,
                                                   NotifyPublisher &notifier)
    : friends_(friends), users_(users), files_(files), messages_(messages),
      sessions_(sessions), notifier_(notifier) {}

zchat::GetFriendListRsp FriendApplicationService::GetFriendList(
    const zchat::GetFriendListReq &request) {
    const std::string user_id = ResolveUserId(
        request.session_id(),
        request.has_user_id() ? request.user_id() : std::string());
    if (user_id.empty()) {
        return ErrorResponse<zchat::GetFriendListRsp>(request.request_id(),
                                                      "登录会话已失效");
    }
    auto ids = friends_.ListFriendIds(user_id);
    if (!ids.ok()) {
        return ErrorResponse<zchat::GetFriendListRsp>(request.request_id(),
                                                      ids.error().message);
    }
    auto users = users_.FindUsersByIds(ids.value());
    if (!users.ok()) {
        return ErrorResponse<zchat::GetFriendListRsp>(request.request_id(),
                                                      users.error().message);
    }
    zchat::GetFriendListRsp response;
    MarkOk(request.request_id(), &response);
    for (const auto &user : users.value()) {
        *response.add_friend_list() = ToProtoUser(user, AvatarForUser(user));
    }
    return response;
}

zchat::GetChatSessionListRsp FriendApplicationService::GetChatSessionList(
    const zchat::GetChatSessionListReq &request) {
    const std::string user_id = ResolveUserId(
        request.session_id(),
        request.has_user_id() ? request.user_id() : std::string());
    if (user_id.empty()) {
        return ErrorResponse<zchat::GetChatSessionListRsp>(request.request_id(),
                                                           "登录会话已失效");
    }
    auto sessions = friends_.ListChatSessions(user_id);
    if (!sessions.ok()) {
        return ErrorResponse<zchat::GetChatSessionListRsp>(
            request.request_id(), sessions.error().message);
    }
    zchat::GetChatSessionListRsp response;
    MarkOk(request.request_id(), &response);
    for (const auto &session : sessions.value()) {
        *response.add_chat_session_info_list() =
            BuildChatSessionInfo(session, user_id);
    }
    return response;
}

zchat::GetPendingFriendEventListRsp
FriendApplicationService::GetPendingFriendEvents(
    const zchat::GetPendingFriendEventListReq &request) {
    const std::string user_id = ResolveUserId(
        request.session_id(),
        request.has_user_id() ? request.user_id() : std::string());
    if (user_id.empty()) {
        return ErrorResponse<zchat::GetPendingFriendEventListRsp>(
            request.request_id(), "登录会话已失效");
    }
    auto applies = friends_.ListPendingApplies(user_id);
    if (!applies.ok()) {
        return ErrorResponse<zchat::GetPendingFriendEventListRsp>(
            request.request_id(), applies.error().message);
    }
    zchat::GetPendingFriendEventListRsp response;
    MarkOk(request.request_id(), &response);
    for (const auto &apply : applies.value()) {
        auto sender = users_.FindUserById(apply.user_id);
        if (sender.ok() && sender.value().has_value()) {
            auto *event = response.add_event();
            event->set_event_id(apply.event_id);
            *event->mutable_sender() = ToProtoUser(
                sender.value().value(), AvatarForUser(sender.value().value()));
        }
    }
    return response;
}

zchat::FriendRemoveRsp
FriendApplicationService::RemoveFriend(const zchat::FriendRemoveReq &request) {
    const std::string user_id = ResolveUserId(
        request.session_id(),
        request.has_user_id() ? request.user_id() : std::string());
    if (user_id.empty()) {
        return ErrorResponse<zchat::FriendRemoveRsp>(request.request_id(),
                                                     "登录会话已失效");
    }
    friends_.DeleteRelation(user_id, request.peer_id());
    friends_.DeleteSingleChatSession(user_id, request.peer_id());

    zchat::NotifyMessage notify;
    notify.set_notify_type(zchat::FRIEND_REMOVE_NOTIFY);
    notify.mutable_friend_remove()->set_user_id(user_id);
    NotifyUser(request.peer_id(), notify);

    zchat::FriendRemoveRsp response;
    MarkOk(request.request_id(), &response);
    return response;
}

zchat::FriendAddRsp
FriendApplicationService::AddFriend(const zchat::FriendAddReq &request) {
    const std::string user_id = ResolveUserId(
        request.session_id(),
        request.has_user_id() ? request.user_id() : std::string());
    if (user_id.empty()) {
        return ErrorResponse<zchat::FriendAddRsp>(request.request_id(),
                                                  "登录会话已失效");
    }
    auto exists = friends_.RelationExists(user_id, request.respondent_id());
    if (exists.ok() && exists.value()) {
        return ErrorResponse<zchat::FriendAddRsp>(request.request_id(),
                                                  "两者已经是好友关系");
    }
    auto applied = friends_.FriendApplyExists(user_id, request.respondent_id());
    if (applied.ok() && applied.value()) {
        return ErrorResponse<zchat::FriendAddRsp>(request.request_id(),
                                                  "已经申请过对方好友");
    }
    const std::string event_id = NewId();
    const auto inserted = friends_.InsertFriendApply(
        FriendApplyRecord{event_id, user_id, request.respondent_id()});
    if (!inserted.ok()) {
        return ErrorResponse<zchat::FriendAddRsp>(request.request_id(),
                                                  inserted.error().message);
    }

    auto sender = users_.FindUserById(user_id);
    if (sender.ok() && sender.value().has_value()) {
        zchat::NotifyMessage notify;
        notify.set_notify_event_id(event_id);
        notify.set_notify_type(zchat::FRIEND_ADD_APPLY_NOTIFY);
        *notify.mutable_friend_add_apply()->mutable_user_info() = ToProtoUser(
            sender.value().value(), AvatarForUser(sender.value().value()));
        NotifyUser(request.respondent_id(), notify);
    }

    zchat::FriendAddRsp response;
    MarkOk(request.request_id(), &response);
    response.set_notify_event_id(event_id);
    return response;
}

zchat::FriendAddProcessRsp FriendApplicationService::ProcessFriendApply(
    const zchat::FriendAddProcessReq &request) {
    const std::string user_id = ResolveUserId(
        request.session_id(),
        request.has_user_id() ? request.user_id() : std::string());
    if (user_id.empty()) {
        return ErrorResponse<zchat::FriendAddProcessRsp>(request.request_id(),
                                                         "登录会话已失效");
    }
    friends_.DeleteFriendApply(request.apply_user_id(), user_id);
    std::string new_session_id;
    if (request.agree()) {
        new_session_id = NewId();
        friends_.InsertRelation(user_id, request.apply_user_id());
        friends_.InsertChatSession(
            ChatSessionRecord{new_session_id, "", ChatSessionType::kSingle});
        friends_.InsertChatSessionMember(new_session_id, user_id);
        friends_.InsertChatSessionMember(new_session_id,
                                         request.apply_user_id());
    }

    auto processor = users_.FindUserById(user_id);
    if (processor.ok() && processor.value().has_value()) {
        zchat::NotifyMessage notify;
        notify.set_notify_type(zchat::FRIEND_ADD_PROCESS_NOTIFY);
        notify.mutable_friend_process_result()->set_agree(request.agree());
        *notify.mutable_friend_process_result()->mutable_user_info() =
            ToProtoUser(processor.value().value(),
                        AvatarForUser(processor.value().value()));
        NotifyUser(request.apply_user_id(), notify);
    }

    zchat::FriendAddProcessRsp response;
    MarkOk(request.request_id(), &response);
    response.set_new_session_id(new_session_id);
    return response;
}

zchat::ChatSessionCreateRsp FriendApplicationService::CreateChatSession(
    const zchat::ChatSessionCreateReq &request) {
    const std::string user_id = ResolveUserId(
        request.session_id(),
        request.has_user_id() ? request.user_id() : std::string());
    if (user_id.empty()) {
        return ErrorResponse<zchat::ChatSessionCreateRsp>(request.request_id(),
                                                          "登录会话已失效");
    }
    const std::string session_id = NewId();
    std::vector<std::string> members(request.member_id_list().begin(),
                                     request.member_id_list().end());
    if (std::find(members.begin(), members.end(), user_id) == members.end()) {
        members.push_back(user_id);
    }
    const std::string name = request.chat_session_name().empty()
                                 ? "新的群聊"
                                 : request.chat_session_name();
    friends_.InsertChatSession(
        ChatSessionRecord{session_id, name, ChatSessionType::kGroup});
    for (const auto &member : members) {
        friends_.InsertChatSessionMember(session_id, member);
    }
    ChatSessionRecord session{session_id, name, ChatSessionType::kGroup};
    zchat::ChatSessionInfo info = BuildChatSessionInfo(session, user_id);
    zchat::NotifyMessage notify;
    notify.set_notify_type(zchat::CHAT_SESSION_CREATE_NOTIFY);
    *notify.mutable_new_chat_session_info()->mutable_chat_session_info() = info;
    for (const auto &member : members) {
        NotifyUser(member, notify);
    }

    zchat::ChatSessionCreateRsp response;
    MarkOk(request.request_id(), &response);
    *response.mutable_chat_session_info() = info;
    return response;
}

zchat::GetChatSessionMemberRsp FriendApplicationService::GetChatSessionMember(
    const zchat::GetChatSessionMemberReq &request) {
    auto ids = friends_.ListChatSessionMembers(request.chat_session_id());
    if (!ids.ok()) {
        return ErrorResponse<zchat::GetChatSessionMemberRsp>(
            request.request_id(), ids.error().message);
    }
    auto users = users_.FindUsersByIds(ids.value());
    if (!users.ok()) {
        return ErrorResponse<zchat::GetChatSessionMemberRsp>(
            request.request_id(), users.error().message);
    }
    zchat::GetChatSessionMemberRsp response;
    MarkOk(request.request_id(), &response);
    for (const auto &user : users.value()) {
        *response.add_member_info_list() =
            ToProtoUser(user, AvatarForUser(user));
    }
    return response;
}

zchat::FriendSearchRsp
FriendApplicationService::SearchFriend(const zchat::FriendSearchReq &request) {
    const std::string user_id = ResolveUserId(
        request.session_id(),
        request.has_user_id() ? request.user_id() : std::string());
    if (user_id.empty()) {
        return ErrorResponse<zchat::FriendSearchRsp>(request.request_id(),
                                                     "登录会话已失效");
    }
    auto users = users_.SearchUsers(request.search_key(), user_id);
    if (!users.ok()) {
        return ErrorResponse<zchat::FriendSearchRsp>(request.request_id(),
                                                     users.error().message);
    }
    zchat::FriendSearchRsp response;
    MarkOk(request.request_id(), &response);
    for (const auto &user : users.value()) {
        auto relation = friends_.RelationExists(user_id, user.user_id);
        if (relation.ok() && relation.value()) {
            continue;
        }
        *response.add_user_info() = ToProtoUser(user, AvatarForUser(user));
    }
    return response;
}

std::string
FriendApplicationService::ResolveUserId(const std::string &session_id,
                                        const std::string &optional_user_id) {
    if (!optional_user_id.empty()) {
        return optional_user_id;
    }
    auto user_id = sessions_.GetUserId(session_id);
    if (!user_id.ok() || !user_id.value().has_value()) {
        return std::string();
    }
    return user_id.value().value();
}

std::string FriendApplicationService::AvatarForUser(const UserRecord &user) {
    if (user.avatar_id.empty()) {
        return std::string();
    }
    auto file = files_.GetFile(user.avatar_id);
    if (!file.ok() || !file.value().has_value()) {
        return std::string();
    }
    return file.value()->file_content;
}

zchat::ChatSessionInfo FriendApplicationService::BuildChatSessionInfo(
    const ChatSessionRecord &session, const std::string &current_user_id) {
    zchat::ChatSessionInfo info;
    info.set_chat_session_id(session.chat_session_id);
    info.set_chat_session_name(session.chat_session_name);
    if (session.chat_session_type == ChatSessionType::kSingle) {
        auto peer_id = friends_.FindSingleChatPeer(session.chat_session_id,
                                                   current_user_id);
        if (peer_id.ok() && peer_id.value().has_value()) {
            info.set_single_chat_friend_id(peer_id.value().value());
            auto peer = users_.FindUserById(peer_id.value().value());
            if (peer.ok() && peer.value().has_value()) {
                info.set_chat_session_name(peer.value()->nickname);
                info.set_avatar(AvatarForUser(peer.value().value()));
            }
        }
    }
    auto last = messages_.LastMessage(session.chat_session_id);
    if (last.ok() && last.value().has_value()) {
        auto sender = users_.FindUserById(last.value()->user_id);
        if (sender.ok() && sender.value().has_value()) {
            *info.mutable_prev_message() = ToProtoMessage(
                last.value().value(), sender.value().value(), "");
        }
    }
    return info;
}

void FriendApplicationService::NotifyUser(const std::string &user_id,
                                          const zchat::NotifyMessage &msg) {
    std::string payload;
    msg.SerializeToString(&payload);
    notifier_.Publish(user_id, payload);
}

} // namespace zchat
