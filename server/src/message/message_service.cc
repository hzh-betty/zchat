#include "message/message_service.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

#include "common/common_errors.h"
#include "common/error_response.h"
#include "common/logger.h"
#include "common/proto_mapper.h"
#include "message/message_errors.h"

namespace zchat {
namespace {} // namespace

MessageService::MessageService(MessageRepository &messages,
                               ServiceClients &clients,
                               MessageSearchIndex &search_index)
    : messages_(messages), clients_(clients), search_index_(search_index) {}

zchat::GetRecentMsgRsp
MessageService::GetRecent(const zchat::GetRecentMsgReq &request) {
    ZCHAT_LOG_INFO("MessageService::GetRecent request_id={}",
                   request.request_id());
    const auto auth = EnsureCanReadSession(
        request.request_id(), request.user_id(), request.chat_session_id());
    if (!auth.ok()) {
        return MakeErrorResponse<zchat::GetRecentMsgRsp>(request.request_id(),
                                                         auth.error());
    }
    auto messages = messages_.ListRecentMessages(
        request.chat_session_id(), static_cast<int>(request.msg_count()));
    if (!messages.ok()) {
        return MakeErrorResponse<zchat::GetRecentMsgRsp>(request.request_id(),
                                                         messages.error());
    }
    auto response = BuildMessageListResponse<zchat::GetRecentMsgRsp>(
        request.request_id(), messages.value());
    ZCHAT_LOG_INFO("MessageService::GetRecent success: request_id={}",
                   request.request_id());
    return response;
}

zchat::GetHistoryMsgRsp
MessageService::GetHistory(const zchat::GetHistoryMsgReq &request) {
    ZCHAT_LOG_INFO("MessageService::GetHistory request_id={}",
                   request.request_id());
    const auto auth = EnsureCanReadSession(
        request.request_id(), request.user_id(), request.chat_session_id());
    if (!auth.ok()) {
        return MakeErrorResponse<zchat::GetHistoryMsgRsp>(request.request_id(),
                                                          auth.error());
    }
    auto messages = messages_.ListMessagesByTime(
        request.chat_session_id(), request.start_time(), request.over_time());
    if (!messages.ok()) {
        return MakeErrorResponse<zchat::GetHistoryMsgRsp>(request.request_id(),
                                                          messages.error());
    }
    auto response = BuildMessageListResponse<zchat::GetHistoryMsgRsp>(
        request.request_id(), messages.value());
    ZCHAT_LOG_INFO("MessageService::GetHistory success: request_id={}",
                   request.request_id());
    return response;
}

zchat::MsgSearchRsp MessageService::Search(const zchat::MsgSearchReq &request) {
    ZCHAT_LOG_INFO("MessageService::Search request_id={}",
                   request.request_id());
    const auto auth = EnsureCanReadSession(
        request.request_id(), request.user_id(), request.chat_session_id());
    if (!auth.ok()) {
        return MakeErrorResponse<zchat::MsgSearchRsp>(request.request_id(),
                                                      auth.error());
    }
    auto messages = search_index_.SearchMessages(request.chat_session_id(),
                                                 request.search_key());
    if (!messages.ok()) {
        return MakeErrorResponse<zchat::MsgSearchRsp>(request.request_id(),
                                                      messages.error());
    }
    auto response = BuildMessageListResponse<zchat::MsgSearchRsp>(
        request.request_id(), messages.value());
    ZCHAT_LOG_INFO("MessageService::Search success: request_id={}",
                   request.request_id());
    return response;
}

VoidResult
MessageService::StoreQueuedMessage(const zchat::MessageInfo &message) {
    std::string file_content;
    MessageRecord record = FromProtoMessage(message, &file_content);
    if (!file_content.empty()) {
        const auto file_id = clients_.PutFile(record.file_name, file_content);
        if (!file_id.ok()) {
            return VoidResult::Fail(file_id.error());
        }
        record.file_id = file_id.value();
    }
    const auto inserted = messages_.InsertMessage(record);
    if (!inserted.ok()) {
        return inserted;
    }
    const auto indexed = search_index_.IndexMessage(record);
    if (!indexed.ok()) {
        return indexed;
    }
    ZCHAT_LOG_INFO("MessageService::StoreQueuedMessage success: message_id={}",
                   record.message_id);
    return VoidResult::Ok();
}

VoidResult MessageService::EnsureCanReadSession(const std::string &,
                                                const std::string &user_id,
                                                const std::string &session_id) {
    if (user_id.empty()) {
        return VoidResult::Fail(common_errors::SessionExpired());
    }
    zchat::GetChatSessionMemberIdsReq req;
    req.set_request_id("internal");
    req.set_chat_session_id(session_id);
    auto rsp = clients_.GetChatSessionMemberIds(req);
    if (!rsp.ok()) {
        return VoidResult::Fail(rsp.error());
    }
    if (!rsp.value().success()) {
        return VoidResult::Fail(AppError::WithCode(
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
        return VoidResult::Fail(message_errors::SessionAccessDenied());
    }
    return VoidResult::Ok();
}

template <typename Response, typename Messages>
Response MessageService::BuildMessageListResponse(const std::string &request_id,
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
    auto user_rsp = clients_.GetMultiUserInfo(user_req);

    std::unordered_map<std::string, std::string> file_contents;
    if (!file_ids.empty()) {
        auto file_rsp = clients_.GetMultiFile(file_ids);
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
            ZCHAT_LOG_WARN("MessageService::BuildMessageListResponse "
                           "user not found: user_id={}",
                           message.user_id);
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
    return response;
}

template zchat::GetRecentMsgRsp
MessageService::BuildMessageListResponse<zchat::GetRecentMsgRsp,
                                         std::vector<MessageRecord>>(
    const std::string &request_id, const std::vector<MessageRecord> &messages);
template zchat::GetHistoryMsgRsp
MessageService::BuildMessageListResponse<zchat::GetHistoryMsgRsp,
                                         std::vector<MessageRecord>>(
    const std::string &request_id, const std::vector<MessageRecord> &messages);
template zchat::MsgSearchRsp
MessageService::BuildMessageListResponse<zchat::MsgSearchRsp,
                                         std::vector<MessageRecord>>(
    const std::string &request_id, const std::vector<MessageRecord> &messages);

} // namespace zchat