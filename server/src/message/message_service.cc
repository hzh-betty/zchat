#include "message/message_service.h"

#include <algorithm>
#include <vector>

#include "common/logger.h"
#include "common/proto_mapper.h"

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

} // namespace

MessageService::MessageService(MessageRepository &messages,
                               UserRepository &users, FileRepository &files,
                               FriendRepository &friends,
                               MessageSearchIndex &search_index)
    : messages_(messages), users_(users), files_(files),
      friends_(friends), search_index_(search_index) {}

zchat::GetRecentMsgRsp
MessageService::GetRecent(const zchat::GetRecentMsgReq &request) {
    ZCHAT_LOG_INFO("MessageService::GetRecent request_id={}", request.request_id());
    const auto auth = EnsureCanReadSession(
        request.request_id(), request.user_id(), request.chat_session_id());
    if (!auth.ok()) {
        ZCHAT_LOG_WARN("MessageService::GetRecent rejected: request_id={} err={}",
                       request.request_id(), auth.error().message);
        return ErrorResponse<zchat::GetRecentMsgRsp>(request.request_id(),
                                                     auth.error().message);
    }
    auto messages = messages_.ListRecentMessages(
        request.chat_session_id(), static_cast<int>(request.msg_count()));
    if (!messages.ok()) {
        ZCHAT_LOG_ERROR("MessageService::GetRecent db failed: {}", messages.error().message);
        return ErrorResponse<zchat::GetRecentMsgRsp>(request.request_id(),
                                                     messages.error().message);
    }
    auto response = BuildMessageListResponse<zchat::GetRecentMsgRsp>(
        request.request_id(), messages.value());
    ZCHAT_LOG_INFO("MessageService::GetRecent success: request_id={}", request.request_id());
    return response;
}

zchat::GetHistoryMsgRsp
MessageService::GetHistory(const zchat::GetHistoryMsgReq &request) {
    ZCHAT_LOG_INFO("MessageService::GetHistory request_id={}", request.request_id());
    const auto auth = EnsureCanReadSession(
        request.request_id(), request.user_id(), request.chat_session_id());
    if (!auth.ok()) {
        ZCHAT_LOG_WARN("MessageService::GetHistory rejected: request_id={} err={}",
                       request.request_id(), auth.error().message);
        return ErrorResponse<zchat::GetHistoryMsgRsp>(request.request_id(),
                                                      auth.error().message);
    }
    auto messages = messages_.ListMessagesByTime(
        request.chat_session_id(), request.start_time(), request.over_time());
    if (!messages.ok()) {
        ZCHAT_LOG_ERROR("MessageService::GetHistory db failed: {}", messages.error().message);
        return ErrorResponse<zchat::GetHistoryMsgRsp>(request.request_id(),
                                                      messages.error().message);
    }
    auto response = BuildMessageListResponse<zchat::GetHistoryMsgRsp>(
        request.request_id(), messages.value());
    ZCHAT_LOG_INFO("MessageService::GetHistory success: request_id={}", request.request_id());
    return response;
}

zchat::MsgSearchRsp MessageService::Search(const zchat::MsgSearchReq &request) {
    ZCHAT_LOG_INFO("MessageService::Search request_id={}", request.request_id());
    const auto auth = EnsureCanReadSession(
        request.request_id(), request.user_id(), request.chat_session_id());
    if (!auth.ok()) {
        ZCHAT_LOG_WARN("MessageService::Search rejected: request_id={} err={}",
                       request.request_id(), auth.error().message);
        return ErrorResponse<zchat::MsgSearchRsp>(request.request_id(),
                                                  auth.error().message);
    }
    auto messages = search_index_.enabled()
                        ? search_index_.SearchMessages(
                              request.chat_session_id(), request.search_key())
                        : messages_.SearchMessages(request.chat_session_id(),
                                                   request.search_key());
    if (!messages.ok()) {
        ZCHAT_LOG_ERROR("MessageService::Search failed: {}", messages.error().message);
        return ErrorResponse<zchat::MsgSearchRsp>(request.request_id(),
                                                  messages.error().message);
    }
    auto response = BuildMessageListResponse<zchat::MsgSearchRsp>(request.request_id(),
                                                         messages.value());
    ZCHAT_LOG_INFO("MessageService::Search success: request_id={}", request.request_id());
    return response;
}

VoidResult MessageService::StoreQueuedMessage(const zchat::MessageInfo &message) {
    std::string file_content;
    MessageRecord record = FromProtoMessage(message, &file_content);
    if (!file_content.empty()) {
        const auto saved = files_.PutFile(FileRecord{
            record.file_id, record.file_name, record.file_size, file_content});
        if (!saved.ok()) {
            ZCHAT_LOG_ERROR("MessageService::StoreQueuedMessage file failed: {}",
                            saved.error().message);
            return saved;
        }
    }
    const auto inserted = messages_.InsertMessage(record);
    if (!inserted.ok()) {
        ZCHAT_LOG_ERROR("MessageService::StoreQueuedMessage db failed: {}",
                        inserted.error().message);
        return inserted;
    }
    const auto indexed = search_index_.IndexMessage(record);
    if (!indexed.ok()) {
        ZCHAT_LOG_ERROR("MessageService::StoreQueuedMessage es failed: {}",
                        indexed.error().message);
        return indexed;
    }
    ZCHAT_LOG_INFO("MessageService::StoreQueuedMessage success: message_id={}",
                   record.message_id);
    return VoidResult::Ok();
}

VoidResult MessageService::EnsureCanReadSession(
    const std::string &, const std::string &user_id,
    const std::string &session_id) {
    if (user_id.empty()) {
        return VoidResult::Fail("登录会话已失效");
    }
    auto members = friends_.ListChatSessionMembers(session_id);
    if (!members.ok()) {
        return VoidResult::Fail(members.error().message);
    }
    if (std::find(members.value().begin(), members.value().end(), user_id) ==
        members.value().end()) {
        return VoidResult::Fail("无权访问该会话消息");
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
    for (const auto &message : messages) {
        auto sender = users_.FindUserById(message.user_id);
        if (!sender.ok() || !sender.value().has_value()) {
            ZCHAT_LOG_WARN("MessageService::BuildMessageListResponse FindUserById failed: user_id={}", message.user_id);
            continue;
        }
        std::string file_content;
        if (!message.file_id.empty()) {
            auto file = files_.GetFile(message.file_id);
            if (file.ok() && file.value().has_value()) {
                file_content = file.value()->file_content;
            }
        }
        *response.add_msg_list() =
            ToProtoMessage(message, sender.value().value(), file_content);
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
