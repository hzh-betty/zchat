#include "transmite/transmite_service.h"

#include <string>

#include "common/common_errors.h"
#include "common/error_response.h"
#include "common/logger.h"
#include "common/proto_mapper.h"
#include "common/uuid.h"
#include "notify.pb.h"
#include "user/user_errors.h"

namespace zchat {
namespace {

} // namespace

TransmiteService::TransmiteService(TransmiteRepository &repository,
                                   UserRepository &users,
                                   MessageQueuePublisher &queue,
                                   MessageSearchIndex &search_index,
                                   SessionStore &sessions,
                                   NotifyPublisher &notifier)
    : repository_(repository), users_(users), queue_(queue),
      search_index_(search_index), sessions_(sessions), notifier_(notifier) {}

zchat::NewMessageRsp
TransmiteService::NewMessage(const zchat::NewMessageReq &request) {
    const auto user_id = sessions_.GetUserId(request.session_id());
    if (!user_id.ok()) {
        return MakeErrorResponse<zchat::NewMessageRsp>(request.request_id(),
                                                       user_id.error());
    }
    if (!user_id.value().has_value()) {
        return MakeErrorResponse<zchat::NewMessageRsp>(
            request.request_id(), common_errors::SessionExpired());
    }

    std::string file_content;
    MessageRecord message =
        ToMessageRecord(request, NewId(), user_id.value().value(),
                        UnixTimeSeconds(), &file_content);
    auto sender = users_.FindUserById(user_id.value().value());
    if (!sender.ok() || !sender.value().has_value()) {
        return MakeErrorResponse<zchat::NewMessageRsp>(
            request.request_id(), user_errors::UserNotFound());
    }

    std::string queue_payload;
    ToProtoMessage(message, sender.value().value(), file_content)
        .SerializeToString(&queue_payload);
    const auto published = queue_.Publish(queue_payload);
    if (!published.ok()) {
        return MakeErrorResponse<zchat::NewMessageRsp>(request.request_id(),
                                                       published.error());
    }

    auto members =
        repository_.ListChatSessionMembers(request.chat_session_id());
    if (members.ok()) {
        zchat::NotifyMessage notify;
        notify.set_notify_type(zchat::CHAT_MESSAGE_NOTIFY);
        *notify.mutable_new_message_info()->mutable_message_info() =
            ToProtoMessage(message, sender.value().value(), file_content);
        std::string payload;
        notify.SerializeToString(&payload);
        for (const auto &member_id : members.value()) {
            if (member_id != user_id.value().value()) {
                notifier_.Publish(member_id, payload);
            }
        }
        ZCHAT_LOG_INFO("new message accepted request={} message={} chat={} "
                       "sender={} targets={}",
                       request.request_id(), message.message_id,
                       request.chat_session_id(), user_id.value().value(),
                       members.value().size() - 1);
    } else {
        ZCHAT_LOG_WARN("new message notify skipped request={} members_ok={}",
                       request.request_id(), members.ok());
    }

    zchat::NewMessageRsp response;
    response.set_request_id(request.request_id());
    response.set_success(true);
    response.set_errmsg("");
    return response;
}

} // namespace zchat
