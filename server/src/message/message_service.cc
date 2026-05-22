#include "message/message_service.h"

#include <vector>

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
                               MessageSearchIndex &search_index)
    : messages_(messages), users_(users), files_(files),
      search_index_(search_index) {}

zchat::GetRecentMsgRsp
MessageService::GetRecent(const zchat::GetRecentMsgReq &request) {
    auto messages = messages_.ListRecentMessages(
        request.chat_session_id(), static_cast<int>(request.msg_count()));
    if (!messages.ok()) {
        return ErrorResponse<zchat::GetRecentMsgRsp>(request.request_id(),
                                                     messages.error().message);
    }
    return BuildMessageListResponse<zchat::GetRecentMsgRsp>(
        request.request_id(), messages.value());
}

zchat::GetHistoryMsgRsp
MessageService::GetHistory(const zchat::GetHistoryMsgReq &request) {
    auto messages = messages_.ListMessagesByTime(
        request.chat_session_id(), request.start_time(), request.over_time());
    if (!messages.ok()) {
        return ErrorResponse<zchat::GetHistoryMsgRsp>(request.request_id(),
                                                      messages.error().message);
    }
    return BuildMessageListResponse<zchat::GetHistoryMsgRsp>(
        request.request_id(), messages.value());
}

zchat::MsgSearchRsp MessageService::Search(const zchat::MsgSearchReq &request) {
    auto messages = search_index_.enabled()
                        ? search_index_.SearchMessages(
                              request.chat_session_id(), request.search_key())
                        : messages_.SearchMessages(request.chat_session_id(),
                                                   request.search_key());
    if (!messages.ok()) {
        return ErrorResponse<zchat::MsgSearchRsp>(request.request_id(),
                                                  messages.error().message);
    }
    return BuildMessageListResponse<zchat::MsgSearchRsp>(request.request_id(),
                                                         messages.value());
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
