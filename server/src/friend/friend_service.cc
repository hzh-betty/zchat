#include "friend/friend_service.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "common/common_errors.h"
#include "common/error_response.h"
#include "common/logger.h"
#include "common/proto_mapper.h"
#include "common/uuid.h"
#include "friend/friend_errors.h"
#include "notify.pb.h"

namespace zchat {
namespace {

template <typename Response>
Response ErrorResponse(const std::string &request_id, const AppError &error) {
    return MakeErrorResponse<Response>(request_id, error);
}

template <typename Response>
void MarkOk(const std::string &request_id, Response *response) {
    response->set_request_id(request_id);
    response->set_success(true);
    response->set_errmsg("");
}

} // namespace

FriendApplicationService::FriendApplicationService(FriendRepository &friends,
                                                   ServiceClients &clients,
                                                   SessionStore &sessions,
                                                   NotifyPublisher &notifier)
    : friends_(friends), clients_(clients), sessions_(sessions),
      notifier_(notifier) {}

drogon::Task<zchat::GetFriendListRsp>
FriendApplicationService::GetFriendListCoro(
    const zchat::GetFriendListReq &request) {
    ZCHAT_LOG_INFO("FriendService::GetFriendList request_id={}",
                   request.request_id());
    const std::string user_id = co_await ResolveUserIdCoro(
        request.session_id(),
        request.has_user_id() ? request.user_id() : std::string());
    if (user_id.empty()) {
        co_return ErrorResponse<zchat::GetFriendListRsp>(
            request.request_id(), common_errors::SessionExpired());
    }
    auto ids = co_await friends_.ListFriendIdsCoro(user_id);
    if (!ids.ok()) {
        co_return ErrorResponse<zchat::GetFriendListRsp>(request.request_id(),
                                                         ids.error());
    }
    zchat::GetMultiUserInfoReq multi_req;
    multi_req.set_request_id(request.request_id());
    for (const auto &id : ids.value()) {
        multi_req.add_users_id(id);
    }
    auto multi_rsp = co_await clients_.GetMultiUserInfoCoro(multi_req);
    zchat::GetFriendListRsp response;
    MarkOk(request.request_id(), &response);
    if (multi_rsp.ok() && multi_rsp.value().success()) {
        for (const auto &id : ids.value()) {
            auto it = multi_rsp.value().users_info().find(id);
            if (it != multi_rsp.value().users_info().end()) {
                *response.add_friend_list() = it->second;
            }
        }
    }
    co_return response;
}

drogon::Task<zchat::GetChatSessionListRsp>
FriendApplicationService::GetChatSessionListCoro(
    const zchat::GetChatSessionListReq &request) {
    ZCHAT_LOG_INFO("FriendService::GetChatSessionList request_id={}",
                   request.request_id());
    const std::string user_id = co_await ResolveUserIdCoro(
        request.session_id(),
        request.has_user_id() ? request.user_id() : std::string());
    if (user_id.empty()) {
        co_return ErrorResponse<zchat::GetChatSessionListRsp>(
            request.request_id(), common_errors::SessionExpired());
    }
    auto sessions = co_await friends_.ListChatSessionsCoro(user_id);
    if (!sessions.ok()) {
        co_return ErrorResponse<zchat::GetChatSessionListRsp>(
            request.request_id(), sessions.error());
    }

    std::vector<std::string> peer_ids;
    std::vector<std::string> session_ids;
    for (const auto &session : sessions.value()) {
        session_ids.push_back(session.chat_session_id);
        if (session.chat_session_type == ChatSessionType::kSingle) {
            auto peer_id = co_await friends_.FindSingleChatPeerCoro(
                session.chat_session_id, user_id);
            if (peer_id.ok() && peer_id.value().has_value()) {
                peer_ids.push_back(peer_id.value().value());
            }
        }
    }

    zchat::GetMultiUserInfoReq multi_user_req;
    multi_user_req.set_request_id(request.request_id());
    for (const auto &pid : peer_ids) {
        multi_user_req.add_users_id(pid);
    }
    auto multi_user_rsp =
        co_await clients_.GetMultiUserInfoCoro(multi_user_req);
    std::unordered_map<std::string, zchat::UserInfo> peer_infos;
    if (multi_user_rsp.ok() && multi_user_rsp.value().success()) {
        for (const auto &pid : peer_ids) {
            auto it = multi_user_rsp.value().users_info().find(pid);
            if (it != multi_user_rsp.value().users_info().end()) {
                peer_infos[pid] = it->second;
            }
        }
    }

    zchat::GetMultiRecentMsgReq multi_recent_req;
    multi_recent_req.set_request_id(request.request_id());
    for (const auto &sid : session_ids) {
        multi_recent_req.add_chat_session_id(sid);
    }
    multi_recent_req.set_msg_count(1);
    auto multi_recent_rsp =
        co_await clients_.GetMultiRecentMsgCoro(multi_recent_req);
    std::unordered_map<std::string, zchat::MessageInfo> recent_msgs;
    if (multi_recent_rsp.ok() && multi_recent_rsp.value().success()) {
        for (const auto &entry : multi_recent_rsp.value().recent_messages()) {
            recent_msgs[entry.first] = entry.second;
        }
    }

    zchat::GetChatSessionListRsp response;
    MarkOk(request.request_id(), &response);
    for (const auto &session : sessions.value()) {
        *response.add_chat_session_info_list() =
            co_await BuildChatSessionInfoCoro(session, user_id, peer_infos,
                                              recent_msgs);
    }
    co_return response;
}

drogon::Task<zchat::GetPendingFriendEventListRsp>
FriendApplicationService::GetPendingFriendEventsCoro(
    const zchat::GetPendingFriendEventListReq &request) {
    ZCHAT_LOG_INFO("FriendService::GetPendingFriendEvents request_id={}",
                   request.request_id());
    const std::string user_id = co_await ResolveUserIdCoro(
        request.session_id(),
        request.has_user_id() ? request.user_id() : std::string());
    if (user_id.empty()) {
        co_return ErrorResponse<zchat::GetPendingFriendEventListRsp>(
            request.request_id(), common_errors::SessionExpired());
    }
    auto applies = co_await friends_.ListPendingAppliesCoro(user_id);
    if (!applies.ok()) {
        co_return ErrorResponse<zchat::GetPendingFriendEventListRsp>(
            request.request_id(), applies.error());
    }
    zchat::GetMultiUserInfoReq multi_req;
    multi_req.set_request_id(request.request_id());
    for (const auto &apply : applies.value()) {
        multi_req.add_users_id(apply.user_id);
    }
    auto multi_rsp = co_await clients_.GetMultiUserInfoCoro(multi_req);
    zchat::GetPendingFriendEventListRsp response;
    MarkOk(request.request_id(), &response);
    for (const auto &apply : applies.value()) {
        auto *event = response.add_event();
        event->set_event_id(apply.event_id);
        if (multi_rsp.ok() && multi_rsp.value().success()) {
            auto it = multi_rsp.value().users_info().find(apply.user_id);
            if (it != multi_rsp.value().users_info().end()) {
                *event->mutable_sender() = it->second;
            }
        }
    }
    co_return response;
}

drogon::Task<zchat::FriendRemoveRsp> FriendApplicationService::RemoveFriendCoro(
    const zchat::FriendRemoveReq &request) {
    ZCHAT_LOG_INFO("FriendService::RemoveFriend request_id={}",
                   request.request_id());
    const std::string user_id = co_await ResolveUserIdCoro(
        request.session_id(),
        request.has_user_id() ? request.user_id() : std::string());
    if (user_id.empty()) {
        co_return ErrorResponse<zchat::FriendRemoveRsp>(
            request.request_id(), common_errors::SessionExpired());
    }
    auto del = co_await friends_.DeleteRelationCoro(user_id, request.peer_id());
    if (!del.ok()) {
        ZCHAT_LOG_WARN("RemoveFriend DeleteRelation failed: {}",
                       del.error().message);
    }
    auto del_session = co_await friends_.DeleteSingleChatSessionCoro(
        user_id, request.peer_id());
    if (!del_session.ok()) {
        ZCHAT_LOG_WARN("RemoveFriend DeleteSingleChatSession failed: {}",
                       del_session.error().message);
    }
    zchat::NotifyMessage notify;
    notify.set_notify_type(zchat::FRIEND_REMOVE_NOTIFY);
    notify.mutable_friend_remove()->set_user_id(user_id);
    co_await NotifyUserCoro(request.peer_id(), notify);
    zchat::FriendRemoveRsp response;
    MarkOk(request.request_id(), &response);
    co_return response;
}

drogon::Task<zchat::FriendAddRsp>
FriendApplicationService::AddFriendCoro(const zchat::FriendAddReq &request) {
    ZCHAT_LOG_INFO("FriendService::AddFriend request_id={}",
                   request.request_id());
    const std::string user_id = co_await ResolveUserIdCoro(
        request.session_id(),
        request.has_user_id() ? request.user_id() : std::string());
    if (user_id.empty()) {
        co_return ErrorResponse<zchat::FriendAddRsp>(
            request.request_id(), common_errors::SessionExpired());
    }
    auto exists =
        co_await friends_.RelationExistsCoro(user_id, request.respondent_id());
    if (exists.ok() && exists.value()) {
        co_return ErrorResponse<zchat::FriendAddRsp>(
            request.request_id(), friend_errors::AlreadyFriends());
    }
    auto applied = co_await friends_.FriendApplyExistsCoro(
        user_id, request.respondent_id());
    if (applied.ok() && applied.value()) {
        co_return ErrorResponse<zchat::FriendAddRsp>(
            request.request_id(), friend_errors::ApplyAlreadyExists());
    }
    const std::string event_id = NewId();
    const auto inserted = co_await friends_.InsertFriendApplyCoro(
        FriendApplyRecord{event_id, user_id, request.respondent_id()});
    if (!inserted.ok()) {
        co_return ErrorResponse<zchat::FriendAddRsp>(request.request_id(),
                                                     inserted.error());
    }
    zchat::UserInfo sender_info = co_await UserInfoForIdCoro(user_id);
    if (!sender_info.user_id().empty()) {
        zchat::NotifyMessage notify;
        notify.set_notify_event_id(event_id);
        notify.set_notify_type(zchat::FRIEND_ADD_APPLY_NOTIFY);
        *notify.mutable_friend_add_apply()->mutable_user_info() = sender_info;
        co_await NotifyUserCoro(request.respondent_id(), notify);
    }
    zchat::FriendAddRsp response;
    MarkOk(request.request_id(), &response);
    response.set_notify_event_id(event_id);
    co_return response;
}

drogon::Task<zchat::FriendAddProcessRsp>
FriendApplicationService::ProcessFriendApplyCoro(
    const zchat::FriendAddProcessReq &request) {
    ZCHAT_LOG_INFO("FriendService::ProcessFriendApply request_id={}",
                   request.request_id());
    const std::string user_id = co_await ResolveUserIdCoro(
        request.session_id(),
        request.has_user_id() ? request.user_id() : std::string());
    if (user_id.empty()) {
        co_return ErrorResponse<zchat::FriendAddProcessRsp>(
            request.request_id(), common_errors::SessionExpired());
    }
    co_await friends_.DeleteFriendApplyCoro(request.apply_user_id(), user_id);
    std::string new_session_id;
    if (request.agree()) {
        new_session_id = NewId();
        auto ins1 = co_await friends_.InsertRelationCoro(
            user_id, request.apply_user_id());
        if (!ins1.ok()) {
            co_return ErrorResponse<zchat::FriendAddProcessRsp>(
                request.request_id(), ins1.error());
        }
        auto ins2 = co_await friends_.InsertChatSessionCoro(
            ChatSessionRecord{new_session_id, "", ChatSessionType::kSingle});
        if (!ins2.ok()) {
            co_return ErrorResponse<zchat::FriendAddProcessRsp>(
                request.request_id(), ins2.error());
        }
        auto add_self = co_await friends_.InsertChatSessionMemberCoro(
            new_session_id, user_id);
        if (!add_self.ok()) {
            co_return ErrorResponse<zchat::FriendAddProcessRsp>(
                request.request_id(), add_self.error());
        }
        auto add_peer = co_await friends_.InsertChatSessionMemberCoro(
            new_session_id, request.apply_user_id());
        if (!add_peer.ok()) {
            co_return ErrorResponse<zchat::FriendAddProcessRsp>(
                request.request_id(), add_peer.error());
        }
    }
    zchat::UserInfo processor_info = co_await UserInfoForIdCoro(user_id);
    if (!processor_info.user_id().empty()) {
        zchat::NotifyMessage notify;
        notify.set_notify_type(zchat::FRIEND_ADD_PROCESS_NOTIFY);
        notify.mutable_friend_process_result()->set_agree(request.agree());
        *notify.mutable_friend_process_result()->mutable_user_info() =
            processor_info;
        co_await NotifyUserCoro(request.apply_user_id(), notify);
    }
    zchat::FriendAddProcessRsp response;
    MarkOk(request.request_id(), &response);
    response.set_new_session_id(new_session_id);
    co_return response;
}

drogon::Task<zchat::ChatSessionCreateRsp>
FriendApplicationService::CreateChatSessionCoro(
    const zchat::ChatSessionCreateReq &request) {
    ZCHAT_LOG_INFO("FriendService::CreateChatSession request_id={}",
                   request.request_id());
    const std::string user_id = co_await ResolveUserIdCoro(
        request.session_id(),
        request.has_user_id() ? request.user_id() : std::string());
    if (user_id.empty()) {
        co_return ErrorResponse<zchat::ChatSessionCreateRsp>(
            request.request_id(), common_errors::SessionExpired());
    }
    const std::string session_id = NewId();
    std::vector<std::string> members(request.member_id_list().begin(),
                                     request.member_id_list().end());
    if (std::find(members.begin(), members.end(), user_id) == members.end()) {
        members.push_back(user_id);
    }
    const std::string name = request.chat_session_name().empty()
                                 ? "New group chat"
                                 : request.chat_session_name();
    auto ins = co_await friends_.InsertChatSessionCoro(
        ChatSessionRecord{session_id, name, ChatSessionType::kGroup});
    if (!ins.ok()) {
        co_return ErrorResponse<zchat::ChatSessionCreateRsp>(
            request.request_id(), ins.error());
    }
    auto ins_member =
        co_await friends_.InsertChatSessionMembersCoro(session_id, members);
    if (!ins_member.ok()) {
        co_return ErrorResponse<zchat::ChatSessionCreateRsp>(
            request.request_id(), ins_member.error());
    }
    ChatSessionRecord session{session_id, name, ChatSessionType::kGroup};
    const std::unordered_map<std::string, zchat::UserInfo> empty_peers;
    const std::unordered_map<std::string, zchat::MessageInfo> empty_msgs;
    zchat::ChatSessionInfo info = co_await BuildChatSessionInfoCoro(
        session, user_id, empty_peers, empty_msgs);
    zchat::NotifyMessage notify;
    notify.set_notify_type(zchat::CHAT_SESSION_CREATE_NOTIFY);
    *notify.mutable_new_chat_session_info()->mutable_chat_session_info() = info;
    co_await NotifyUsersCoro(members, notify);
    zchat::ChatSessionCreateRsp response;
    MarkOk(request.request_id(), &response);
    *response.mutable_chat_session_info() = info;
    co_return response;
}

drogon::Task<zchat::GetChatSessionMemberRsp>
FriendApplicationService::GetChatSessionMemberCoro(
    const zchat::GetChatSessionMemberReq &request) {
    ZCHAT_LOG_INFO("FriendService::GetChatSessionMember request_id={}",
                   request.request_id());
    auto ids =
        co_await friends_.ListChatSessionMembersCoro(request.chat_session_id());
    if (!ids.ok()) {
        co_return ErrorResponse<zchat::GetChatSessionMemberRsp>(
            request.request_id(), ids.error());
    }
    zchat::GetMultiUserInfoReq multi_req;
    multi_req.set_request_id(request.request_id());
    for (const auto &id : ids.value()) {
        multi_req.add_users_id(id);
    }
    zchat::GetChatSessionMemberRsp response;
    MarkOk(request.request_id(), &response);
    auto multi_rsp = co_await clients_.GetMultiUserInfoCoro(multi_req);
    if (multi_rsp.ok() && multi_rsp.value().success()) {
        for (const auto &id : ids.value()) {
            auto it = multi_rsp.value().users_info().find(id);
            if (it != multi_rsp.value().users_info().end()) {
                *response.add_member_info_list() = it->second;
            }
        }
    }
    co_return response;
}

drogon::Task<zchat::GetChatSessionMemberIdsRsp>
FriendApplicationService::GetChatSessionMemberIdsCoro(
    const zchat::GetChatSessionMemberIdsReq &request) {
    auto ids =
        co_await friends_.ListChatSessionMembersCoro(request.chat_session_id());
    if (!ids.ok()) {
        zchat::GetChatSessionMemberIdsRsp response;
        response.set_request_id(request.request_id());
        response.set_success(false);
        response.set_errmsg(FormatErrorForClient(ids.error()));
        co_return response;
    }
    zchat::GetChatSessionMemberIdsRsp response;
    response.set_request_id(request.request_id());
    response.set_success(true);
    response.set_errmsg("");
    for (const auto &id : ids.value()) {
        response.add_member_id(id);
    }
    co_return response;
}

drogon::Task<zchat::FriendSearchRsp> FriendApplicationService::SearchFriendCoro(
    const zchat::FriendSearchReq &request) {
    ZCHAT_LOG_INFO("FriendService::SearchFriend request_id={}",
                   request.request_id());
    const std::string user_id = co_await ResolveUserIdCoro(
        request.session_id(),
        request.has_user_id() ? request.user_id() : std::string());
    if (user_id.empty()) {
        co_return ErrorResponse<zchat::FriendSearchRsp>(
            request.request_id(), common_errors::SessionExpired());
    }
    zchat::SearchUsersReq search_request;
    search_request.set_request_id(request.request_id());
    search_request.set_search_key(request.search_key());
    search_request.set_exclude_user_id(user_id);
    auto search_response = co_await clients_.SearchUsersCoro(search_request);
    if (!search_response.ok()) {
        co_return ErrorResponse<zchat::FriendSearchRsp>(
            request.request_id(), search_response.error());
    }
    zchat::FriendSearchRsp response;
    MarkOk(request.request_id(), &response);
    std::vector<std::string> candidate_ids;
    for (const auto &user_info : search_response.value().user_info()) {
        candidate_ids.push_back(user_info.user_id());
    }
    std::unordered_set<std::string> existing_set;
    auto existing =
        co_await friends_.ListExistingPeersCoro(user_id, candidate_ids);
    if (existing.ok()) {
        for (const auto &pid : existing.value()) {
            existing_set.insert(pid);
        }
    }
    for (const auto &user_info : search_response.value().user_info()) {
        if (existing_set.count(user_info.user_id()) > 0) {
            continue;
        }
        *response.add_user_info() = user_info;
    }
    co_return response;
}

drogon::Task<std::string> FriendApplicationService::ResolveUserIdCoro(
    const std::string &session_id, const std::string &optional_user_id) {
    auto user_id = co_await sessions_.GetUserIdCoro(session_id);
    if (!user_id.ok() || !user_id.value().has_value()) {
        co_return std::string();
    }
    const std::string &resolved = user_id.value().value();
    if (!optional_user_id.empty() && optional_user_id != resolved) {
        co_return std::string();
    }
    co_return resolved;
}

drogon::Task<zchat::UserInfo>
FriendApplicationService::UserInfoForIdCoro(const std::string &user_id) {
    zchat::GetUserInfoReq req;
    req.set_user_id(user_id);
    auto rsp = co_await clients_.GetUserCoro(req);
    if (!rsp.ok() || !rsp.value().success()) {
        co_return zchat::UserInfo{};
    }
    co_return rsp.value().user_info();
}

drogon::Task<zchat::ChatSessionInfo>
FriendApplicationService::BuildChatSessionInfoCoro(
    const ChatSessionRecord &session, const std::string &current_user_id,
    const std::unordered_map<std::string, zchat::UserInfo> &peer_infos,
    const std::unordered_map<std::string, zchat::MessageInfo> &recent_msgs) {
    zchat::ChatSessionInfo info;
    info.set_chat_session_id(session.chat_session_id);
    info.set_chat_session_name(session.chat_session_name);
    if (session.chat_session_type == ChatSessionType::kSingle) {
        auto peer_id = co_await friends_.FindSingleChatPeerCoro(
            session.chat_session_id, current_user_id);
        if (peer_id.ok() && peer_id.value().has_value()) {
            info.set_single_chat_friend_id(peer_id.value().value());
            auto it = peer_infos.find(peer_id.value().value());
            if (it != peer_infos.end()) {
                info.set_chat_session_name(it->second.nickname());
                info.set_avatar(it->second.avatar());
            }
        }
    }
    auto msg_it = recent_msgs.find(session.chat_session_id);
    if (msg_it != recent_msgs.end()) {
        *info.mutable_prev_message() = msg_it->second;
    }
    co_return info;
}

drogon::Task<VoidResult>
FriendApplicationService::NotifyUserCoro(const std::string &user_id,
                                         const zchat::NotifyMessage &msg) {
    std::string payload;
    msg.SerializeToString(&payload);
    co_return co_await notifier_.PublishCoro(user_id, payload);
}

drogon::Task<VoidResult> FriendApplicationService::NotifyUsersCoro(
    const std::vector<std::string> &user_ids, const zchat::NotifyMessage &msg) {
    std::string payload;
    msg.SerializeToString(&payload);
    auto outcome = co_await notifier_.PublishBatchCoro(user_ids, payload);
    if (outcome.ok()) {
        for (const auto &failed_id : outcome.value().failed) {
            ZCHAT_LOG_WARN("NotifyUsers publish failed user={}", failed_id);
        }
    } else {
        ZCHAT_LOG_WARN("NotifyUsers publish batch failed error={} total={}",
                       outcome.error().message, user_ids.size());
    }
    co_return VoidResult::Ok();
}

} // namespace zchat
