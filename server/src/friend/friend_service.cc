#include "friend/friend_service.h"

#include <algorithm>
#include <vector>

#include "common/logger.h"
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
                                                   NotifyPublisher &notifier,
                                                   UserSearchIndex &search_index)
    : friends_(friends), users_(users), files_(files), messages_(messages),
      sessions_(sessions), notifier_(notifier), search_index_(search_index) {}

zchat::GetFriendListRsp FriendApplicationService::GetFriendList(
    const zchat::GetFriendListReq &request) {
    ZCHAT_LOG_INFO("FriendService::GetFriendList request_id={}", request.request_id());
    const std::string user_id = ResolveUserId(
        request.session_id(),
        request.has_user_id() ? request.user_id() : std::string());
    if (user_id.empty()) {
        ZCHAT_LOG_WARN("FriendService::GetFriendList rejected: request_id={} reason={}", request.request_id(), "登录会话已失效");
        return ErrorResponse<zchat::GetFriendListRsp>(request.request_id(),
                                                      "登录会话已失效");
    }
    auto ids = friends_.ListFriendIds(user_id);
    if (!ids.ok()) {
        ZCHAT_LOG_ERROR("FriendService::GetFriendList db failed: {}", ids.error().message);
        return ErrorResponse<zchat::GetFriendListRsp>(request.request_id(),
                                                      ids.error().message);
    }
    auto users = users_.FindUsersByIds(ids.value());
    if (!users.ok()) {
        ZCHAT_LOG_ERROR("FriendService::GetFriendList db failed: {}", users.error().message);
        return ErrorResponse<zchat::GetFriendListRsp>(request.request_id(),
                                                      users.error().message);
    }
    zchat::GetFriendListRsp response;
    MarkOk(request.request_id(), &response);
    for (const auto &user : users.value()) {
        *response.add_friend_list() = ToProtoUser(user, AvatarForUser(user));
    }
    ZCHAT_LOG_INFO("FriendService::GetFriendList success: request_id={}", request.request_id());
    return response;
}

zchat::GetChatSessionListRsp FriendApplicationService::GetChatSessionList(
    const zchat::GetChatSessionListReq &request) {
    ZCHAT_LOG_INFO("FriendService::GetChatSessionList request_id={}", request.request_id());
    const std::string user_id = ResolveUserId(
        request.session_id(),
        request.has_user_id() ? request.user_id() : std::string());
    if (user_id.empty()) {
        ZCHAT_LOG_WARN("FriendService::GetChatSessionList rejected: request_id={} reason={}", request.request_id(), "登录会话已失效");
        return ErrorResponse<zchat::GetChatSessionListRsp>(request.request_id(),
                                                           "登录会话已失效");
    }
    auto sessions = friends_.ListChatSessions(user_id);
    if (!sessions.ok()) {
        ZCHAT_LOG_ERROR("FriendService::GetChatSessionList db failed: {}", sessions.error().message);
        return ErrorResponse<zchat::GetChatSessionListRsp>(
            request.request_id(), sessions.error().message);
    }
    zchat::GetChatSessionListRsp response;
    MarkOk(request.request_id(), &response);
    for (const auto &session : sessions.value()) {
        *response.add_chat_session_info_list() =
            BuildChatSessionInfo(session, user_id);
    }
    ZCHAT_LOG_INFO("FriendService::GetChatSessionList success: request_id={}", request.request_id());
    return response;
}

zchat::GetPendingFriendEventListRsp
FriendApplicationService::GetPendingFriendEvents(
    const zchat::GetPendingFriendEventListReq &request) {
    ZCHAT_LOG_INFO("FriendService::GetPendingFriendEvents request_id={}", request.request_id());
    const std::string user_id = ResolveUserId(
        request.session_id(),
        request.has_user_id() ? request.user_id() : std::string());
    if (user_id.empty()) {
        ZCHAT_LOG_WARN("FriendService::GetPendingFriendEvents rejected: request_id={} reason={}", request.request_id(), "登录会话已失效");
        return ErrorResponse<zchat::GetPendingFriendEventListRsp>(
            request.request_id(), "登录会话已失效");
    }
    auto applies = friends_.ListPendingApplies(user_id);
    if (!applies.ok()) {
        ZCHAT_LOG_ERROR("FriendService::GetPendingFriendEvents db failed: {}", applies.error().message);
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
    ZCHAT_LOG_INFO("FriendService::GetPendingFriendEvents success: request_id={}", request.request_id());
    return response;
}

zchat::FriendRemoveRsp
FriendApplicationService::RemoveFriend(const zchat::FriendRemoveReq &request) {
    ZCHAT_LOG_INFO("FriendService::RemoveFriend request_id={}", request.request_id());
    const std::string user_id = ResolveUserId(
        request.session_id(),
        request.has_user_id() ? request.user_id() : std::string());
    if (user_id.empty()) {
        ZCHAT_LOG_WARN("FriendService::RemoveFriend rejected: request_id={} reason={}", request.request_id(), "登录会话已失效");
        return ErrorResponse<zchat::FriendRemoveRsp>(request.request_id(),
                                                     "登录会话已失效");
    }
    auto del = friends_.DeleteRelation(user_id, request.peer_id());
    if (!del.ok()) {
        ZCHAT_LOG_WARN("FriendService::RemoveFriend DeleteRelation failed: request_id={} err={}", request.request_id(), del.error().message);
    }
    auto del_session = friends_.DeleteSingleChatSession(user_id, request.peer_id());
    if (!del_session.ok()) {
        ZCHAT_LOG_WARN("FriendService::RemoveFriend DeleteSingleChatSession failed: request_id={} err={}", request.request_id(), del_session.error().message);
    }

    zchat::NotifyMessage notify;
    notify.set_notify_type(zchat::FRIEND_REMOVE_NOTIFY);
    notify.mutable_friend_remove()->set_user_id(user_id);
    NotifyUser(request.peer_id(), notify);

    zchat::FriendRemoveRsp response;
    MarkOk(request.request_id(), &response);
    ZCHAT_LOG_INFO("FriendService::RemoveFriend success: request_id={} peer_id={}", request.request_id(), request.peer_id());
    return response;
}

zchat::FriendAddRsp
FriendApplicationService::AddFriend(const zchat::FriendAddReq &request) {
    ZCHAT_LOG_INFO("FriendService::AddFriend request_id={}", request.request_id());
    const std::string user_id = ResolveUserId(
        request.session_id(),
        request.has_user_id() ? request.user_id() : std::string());
    if (user_id.empty()) {
        ZCHAT_LOG_WARN("FriendService::AddFriend rejected: request_id={} reason={}", request.request_id(), "登录会话已失效");
        return ErrorResponse<zchat::FriendAddRsp>(request.request_id(),
                                                  "登录会话已失效");
    }
    auto exists = friends_.RelationExists(user_id, request.respondent_id());
    if (exists.ok() && exists.value()) {
        ZCHAT_LOG_WARN("FriendService::AddFriend rejected: request_id={} reason={}", request.request_id(), "两者已经是好友关系");
        return ErrorResponse<zchat::FriendAddRsp>(request.request_id(),
                                                  "两者已经是好友关系");
    }
    auto applied = friends_.FriendApplyExists(user_id, request.respondent_id());
    if (applied.ok() && applied.value()) {
        ZCHAT_LOG_WARN("FriendService::AddFriend rejected: request_id={} reason={}", request.request_id(), "已经申请过对方好友");
        return ErrorResponse<zchat::FriendAddRsp>(request.request_id(),
                                                  "已经申请过对方好友");
    }
    const std::string event_id = NewId();
    const auto inserted = friends_.InsertFriendApply(
        FriendApplyRecord{event_id, user_id, request.respondent_id()});
    if (!inserted.ok()) {
        ZCHAT_LOG_ERROR("FriendService::AddFriend db failed: {}", inserted.error().message);
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
    ZCHAT_LOG_INFO("FriendService::AddFriend success: request_id={}", request.request_id());
    return response;
}

zchat::FriendAddProcessRsp FriendApplicationService::ProcessFriendApply(
    const zchat::FriendAddProcessReq &request) {
    ZCHAT_LOG_INFO("FriendService::ProcessFriendApply request_id={}", request.request_id());
    const std::string user_id = ResolveUserId(
        request.session_id(),
        request.has_user_id() ? request.user_id() : std::string());
    if (user_id.empty()) {
        ZCHAT_LOG_WARN("FriendService::ProcessFriendApply rejected: request_id={} reason={}", request.request_id(), "登录会话已失效");
        return ErrorResponse<zchat::FriendAddProcessRsp>(request.request_id(),
                                                         "登录会话已失效");
    }
    friends_.DeleteFriendApply(request.apply_user_id(), user_id);
    std::string new_session_id;
    if (request.agree()) {
        new_session_id = NewId();
        auto ins1 = friends_.InsertRelation(user_id, request.apply_user_id());
        if (!ins1.ok()) {
            ZCHAT_LOG_ERROR("FriendService::ProcessFriendApply InsertRelation failed: {}", ins1.error().message);
        }
        auto ins2 = friends_.InsertChatSession(
            ChatSessionRecord{new_session_id, "", ChatSessionType::kSingle});
        if (!ins2.ok()) {
            ZCHAT_LOG_ERROR("FriendService::ProcessFriendApply InsertChatSession failed: {}", ins2.error().message);
        }
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
    ZCHAT_LOG_INFO("FriendService::ProcessFriendApply success: request_id={} agree={}", request.request_id(), request.agree());
    return response;
}

zchat::ChatSessionCreateRsp FriendApplicationService::CreateChatSession(
    const zchat::ChatSessionCreateReq &request) {
    ZCHAT_LOG_INFO("FriendService::CreateChatSession request_id={}", request.request_id());
    const std::string user_id = ResolveUserId(
        request.session_id(),
        request.has_user_id() ? request.user_id() : std::string());
    if (user_id.empty()) {
        ZCHAT_LOG_WARN("FriendService::CreateChatSession rejected: request_id={} reason={}", request.request_id(), "登录会话已失效");
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
    auto ins = friends_.InsertChatSession(
        ChatSessionRecord{session_id, name, ChatSessionType::kGroup});
    if (!ins.ok()) {
        ZCHAT_LOG_ERROR("FriendService::CreateChatSession db failed: {}", ins.error().message);
    }
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
    ZCHAT_LOG_INFO("FriendService::CreateChatSession success: request_id={}", request.request_id());
    return response;
}

zchat::GetChatSessionMemberRsp FriendApplicationService::GetChatSessionMember(
    const zchat::GetChatSessionMemberReq &request) {
    ZCHAT_LOG_INFO("FriendService::GetChatSessionMember request_id={}", request.request_id());
    auto ids = friends_.ListChatSessionMembers(request.chat_session_id());
    if (!ids.ok()) {
        ZCHAT_LOG_ERROR("FriendService::GetChatSessionMember db failed: {}", ids.error().message);
        return ErrorResponse<zchat::GetChatSessionMemberRsp>(
            request.request_id(), ids.error().message);
    }
    auto users = users_.FindUsersByIds(ids.value());
    if (!users.ok()) {
        ZCHAT_LOG_ERROR("FriendService::GetChatSessionMember db failed: {}", users.error().message);
        return ErrorResponse<zchat::GetChatSessionMemberRsp>(
            request.request_id(), users.error().message);
    }
    zchat::GetChatSessionMemberRsp response;
    MarkOk(request.request_id(), &response);
    for (const auto &user : users.value()) {
        *response.add_member_info_list() =
            ToProtoUser(user, AvatarForUser(user));
    }
    ZCHAT_LOG_INFO("FriendService::GetChatSessionMember success: request_id={}", request.request_id());
    return response;
}

zchat::FriendSearchRsp
FriendApplicationService::SearchFriend(const zchat::FriendSearchReq &request) {
    ZCHAT_LOG_INFO("FriendService::SearchFriend request_id={}", request.request_id());
    const std::string user_id = ResolveUserId(
        request.session_id(),
        request.has_user_id() ? request.user_id() : std::string());
    if (user_id.empty()) {
        ZCHAT_LOG_WARN("FriendService::SearchFriend rejected: request_id={} reason={}", request.request_id(), "登录会话已失效");
        return ErrorResponse<zchat::FriendSearchRsp>(request.request_id(),
                                                     "登录会话已失效");
    }
    auto excluded = friends_.ListFriendIds(user_id);
    if (!excluded.ok()) {
        ZCHAT_LOG_ERROR("FriendService::SearchFriend friend list failed: {}",
                        excluded.error().message);
        return ErrorResponse<zchat::FriendSearchRsp>(request.request_id(),
                                                     excluded.error().message);
    }
    excluded.value().push_back(user_id);
    auto users = users_.SearchUsers(request.search_key(), user_id);
    if (search_index_.enabled()) {
        users = search_index_.SearchUsers(request.search_key(),
                                          excluded.value());
        if (!users.ok()) {
            ZCHAT_LOG_WARN(
                "FriendService::SearchFriend es failed, fallback mysql: {}",
                users.error().message);
            users = users_.SearchUsers(request.search_key(), user_id);
        } else if (users.value().empty()) {
            ZCHAT_LOG_DEBUG(
                "FriendService::SearchFriend es empty, fallback mysql");
            users = users_.SearchUsers(request.search_key(), user_id);
        }
    }
    if (!users.ok()) {
        ZCHAT_LOG_ERROR("FriendService::SearchFriend failed: {}", users.error().message);
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
    ZCHAT_LOG_INFO("FriendService::SearchFriend success: request_id={}", request.request_id());
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
