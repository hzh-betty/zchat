#include "message/message_service.h"

#include <algorithm>
#include <optional>
#include <unordered_map>
#include <vector>

#include "common/common_errors.h"
#include "common/error_response.h"
#include "common/logger.h"
#include "common/proto_mapper.h"
#include "message/message_errors.h"

namespace zchat {

MessageService::MessageService(MessageRepository &messages,
                               ServiceClients &clients,
                               MessageSearchIndex &search_index)
    : messages_(messages), clients_(clients), search_index_(search_index) {}

drogon::Task<zchat::GetRecentMsgRsp>
MessageService::GetRecentCoro(const zchat::GetRecentMsgReq &request) {
    ZCHAT_LOG_INFO("MessageService::GetRecent request_id={}",
                   request.request_id());
    const auto auth = co_await EnsureCanReadSessionCoro(
        request.user_id(), request.chat_session_id());
    if (!auth.ok()) {
        co_return MakeErrorResponse<zchat::GetRecentMsgRsp>(
            request.request_id(), auth.error());
    }
    auto messages = co_await messages_.ListRecentMessagesCoro(
        request.chat_session_id(), static_cast<int>(request.msg_count()));
    if (!messages.ok()) {
        co_return MakeErrorResponse<zchat::GetRecentMsgRsp>(
            request.request_id(), messages.error());
    }
    co_return co_await BuildMessageListResponseCoro<zchat::GetRecentMsgRsp>(
        request.request_id(), messages.value());
}

drogon::Task<zchat::GetMultiRecentMsgRsp>
MessageService::GetMultiRecentCoro(const zchat::GetMultiRecentMsgReq &request) {
    ZCHAT_LOG_INFO("MessageService::GetMultiRecent request_id={} sessions={}",
                   request.request_id(), request.chat_session_id_size());
    // GetMultiRecent 是内部调用（friend 服务），不做调用方校验
    std::vector<std::string> session_ids(request.chat_session_id().begin(),
                                         request.chat_session_id().end());
    auto messages =
        co_await messages_.ListLastMessagesForSessionsCoro(session_ids);
    if (!messages.ok()) {
        co_return MakeErrorResponse<zchat::GetMultiRecentMsgRsp>(
            request.request_id(), messages.error());
    }

    std::vector<std::string> user_ids;
    std::vector<std::string> file_ids;
    for (const auto &message : messages.value()) {
        user_ids.push_back(message.user_id);
        if (!message.file_id.empty()) {
            file_ids.push_back(message.file_id);
        }
    }
    zchat::GetMultiUserInfoReq user_req;
    user_req.set_request_id(request.request_id());
    for (const auto &id : user_ids) {
        user_req.add_users_id(id);
    }
    auto user_rsp = co_await clients_.GetMultiUserInfoCoro(user_req);
    std::unordered_map<std::string, std::string> file_contents;
    if (!file_ids.empty()) {
        auto file_rsp = co_await clients_.GetMultiFileCoro(file_ids);
        if (file_rsp.ok() && file_rsp.value().success()) {
            for (const auto &fd : file_rsp.value().file_data()) {
                file_contents[fd.file_id()] = fd.file_content();
            }
        }
    }

    zchat::GetMultiRecentMsgRsp response;
    response.set_request_id(request.request_id());
    response.set_success(true);
    response.set_errmsg("");
    for (const auto &message : messages.value()) {
        zchat::UserInfo sender;
        if (user_rsp.ok() && user_rsp.value().success()) {
            auto it = user_rsp.value().users_info().find(message.user_id);
            if (it != user_rsp.value().users_info().end()) {
                sender = it->second;
            }
        }
        if (sender.user_id().empty()) {
            continue;
        }
        std::string file_content;
        if (!message.file_id.empty()) {
            auto it = file_contents.find(message.file_id);
            if (it != file_contents.end()) {
                file_content = it->second;
            }
        }
        (*response.mutable_recent_messages())[message.session_id] =
            ToProtoMessage(message, FromProtoUser(sender), file_content);
    }
    co_return response;
}

drogon::Task<zchat::GetHistoryMsgRsp>
MessageService::GetHistoryCoro(const zchat::GetHistoryMsgReq &request) {
    ZCHAT_LOG_INFO("MessageService::GetHistory request_id={}",
                   request.request_id());
    const auto auth = co_await EnsureCanReadSessionCoro(
        request.user_id(), request.chat_session_id());
    if (!auth.ok()) {
        co_return MakeErrorResponse<zchat::GetHistoryMsgRsp>(
            request.request_id(), auth.error());
    }
    int max_count =
        request.has_max_count() ? static_cast<int>(request.max_count()) : 50;
    if (max_count < 1) {
        max_count = 1;
    }
    if (max_count > 200) {
        max_count = 200;
    }
    std::optional<std::string> before_msg_id;
    if (request.has_before_msg_id() && !request.before_msg_id().empty()) {
        before_msg_id = request.before_msg_id();
    }
    auto messages = co_await messages_.ListMessagesByTimeCoro(
        request.chat_session_id(), request.start_time(), request.over_time(),
        max_count, before_msg_id);
    if (!messages.ok()) {
        co_return MakeErrorResponse<zchat::GetHistoryMsgRsp>(
            request.request_id(), messages.error());
    }
    co_return co_await BuildMessageListResponseCoro<zchat::GetHistoryMsgRsp>(
        request.request_id(), messages.value());
}

drogon::Task<zchat::MsgSearchRsp>
MessageService::SearchCoro(const zchat::MsgSearchReq &request) {
    ZCHAT_LOG_INFO("MessageService::Search request_id={}",
                   request.request_id());
    const auto auth = co_await EnsureCanReadSessionCoro(
        request.user_id(), request.chat_session_id());
    if (!auth.ok()) {
        co_return MakeErrorResponse<zchat::MsgSearchRsp>(request.request_id(),
                                                         auth.error());
    }
    int offset = request.has_offset() ? request.offset() : 0;
    if (offset < 0) {
        offset = 0;
    }
    int limit = request.has_limit() ? request.limit() : 50;
    if (limit < 1) {
        limit = 1;
    }
    if (limit > 100) {
        limit = 100;
    }
    auto messages = co_await search_index_.SearchMessagesCoro(
        request.chat_session_id(), request.search_key(), offset, limit);
    if (!messages.ok()) {
        co_return MakeErrorResponse<zchat::MsgSearchRsp>(request.request_id(),
                                                         messages.error());
    }
    co_return co_await BuildMessageListResponseCoro<zchat::MsgSearchRsp>(
        request.request_id(), messages.value());
}

drogon::Task<VoidResult>
MessageService::StoreQueuedMessageCoro(const zchat::MessageInfo &message) {
    std::string file_content;
    MessageRecord record = FromProtoMessage(message, &file_content);
    if (!file_content.empty()) {
        const auto file_id =
            co_await clients_.PutFileCoro(record.file_name, file_content);
        if (!file_id.ok()) {
            co_return VoidResult::Fail(file_id.error());
        }
        record.file_id = file_id.value();
    }
    const auto inserted = co_await messages_.InsertMessageCoro(record);
    if (!inserted.ok()) {
        co_return inserted;
    }
    const auto indexed = co_await search_index_.IndexMessageCoro(record);
    if (!indexed.ok()) {
        co_return indexed;
    }
    ZCHAT_LOG_INFO("StoreQueuedMessage success: message_id={}",
                   record.message_id);
    co_return VoidResult::Ok();
}

drogon::Task<VoidResult>
MessageService::EnsureCanReadSessionCoro(const std::string &user_id,
                                         const std::string &session_id) {
    if (user_id.empty()) {
        co_return VoidResult::Fail(common_errors::SessionExpired());
    }
    zchat::GetChatSessionMemberIdsReq req;
    req.set_request_id("internal");
    req.set_chat_session_id(session_id);
    auto rsp = co_await clients_.GetChatSessionMemberIdsCoro(req);
    if (!rsp.ok()) {
        co_return VoidResult::Fail(rsp.error());
    }
    if (!rsp.value().success()) {
        co_return VoidResult::Fail(AppError::WithCode(
            ErrorCode::kExternalServiceError,
            "friend service GetChatSessionMemberIds failed"));
    }
    bool is_member = false;
    for (const auto &member_id : rsp.value().member_id()) {
        if (member_id == user_id) {
            is_member = true;
            break;
        }
    }
    if (!is_member) {
        co_return VoidResult::Fail(message_errors::SessionAccessDenied());
    }
    co_return VoidResult::Ok();
}

template <typename Response, typename Messages>
drogon::Task<Response>
MessageService::BuildMessageListResponseCoro(const std::string &request_id,
                                             const Messages &messages) {
    Response response;
    response.set_request_id(request_id);
    response.set_success(true);
    response.set_errmsg("");

    std::vector<std::string> user_ids;
    std::vector<std::string> file_ids;
    for (const auto &message : messages) {
        user_ids.push_back(message.user_id);
        if (!message.file_id.empty()) {
            file_ids.push_back(message.file_id);
        }
    }

    zchat::GetMultiUserInfoReq user_req;
    user_req.set_request_id(request_id);
    for (const auto &id : user_ids) {
        user_req.add_users_id(id);
    }
    auto user_rsp = co_await clients_.GetMultiUserInfoCoro(user_req);

    std::unordered_map<std::string, std::string> file_contents;
    if (!file_ids.empty()) {
        auto file_rsp = co_await clients_.GetMultiFileCoro(file_ids);
        if (file_rsp.ok() && file_rsp.value().success()) {
            for (const auto &fd : file_rsp.value().file_data()) {
                file_contents[fd.file_id()] = fd.file_content();
            }
        }
    }

    for (const auto &message : messages) {
        zchat::UserInfo sender;
        if (user_rsp.ok() && user_rsp.value().success()) {
            auto it = user_rsp.value().users_info().find(message.user_id);
            if (it != user_rsp.value().users_info().end()) {
                sender = it->second;
            }
        }
        if (sender.user_id().empty()) {
            continue;
        }
        std::string file_content;
        if (!message.file_id.empty()) {
            auto it = file_contents.find(message.file_id);
            if (it != file_contents.end()) {
                file_content = it->second;
            }
        }
        *response.add_msg_list() =
            ToProtoMessage(message, FromProtoUser(sender), file_content);
    }
    co_return response;
}

template drogon::Task<zchat::GetRecentMsgRsp>
MessageService::BuildMessageListResponseCoro<zchat::GetRecentMsgRsp,
                                             std::vector<MessageRecord>>(
    const std::string &request_id, const std::vector<MessageRecord> &messages);

template drogon::Task<zchat::GetHistoryMsgRsp>
MessageService::BuildMessageListResponseCoro<zchat::GetHistoryMsgRsp,
                                             std::vector<MessageRecord>>(
    const std::string &request_id, const std::vector<MessageRecord> &messages);

template drogon::Task<zchat::MsgSearchRsp>
MessageService::BuildMessageListResponseCoro<zchat::MsgSearchRsp,
                                             std::vector<MessageRecord>>(
    const std::string &request_id, const std::vector<MessageRecord> &messages);

} // namespace zchat
