#include "transmite/transmite_service.h"

#include <future>
#include <string>
#include <vector>

#include "common/common_errors.h"
#include "common/error_response.h"
#include "common/logger.h"
#include "common/proto_mapper.h"
#include "common/uuid.h"
#include "notify.pb.h"

namespace zchat {
namespace {} // namespace

TransmiteService::TransmiteService(MessageQueuePublisher &queue,
                                   SessionStore &sessions,
                                   NotifyPublisher &notifier,
                                   ServiceClients &clients)
    : queue_(queue), sessions_(sessions), notifier_(notifier),
      clients_(clients) {}

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

    zchat::GetUserInfoReq user_request;
    user_request.set_user_id(user_id.value().value());
    auto user_response = clients_.GetUser(user_request);
    if (!user_response.ok() || !user_response.value().success()) {
        AppError error = common_errors::InternalServiceError();
        if (!user_response.ok()) {
            error = user_response.error();
        }
        return MakeErrorResponse<zchat::NewMessageRsp>(request.request_id(),
                                                       error);
    }
    const zchat::UserInfo &sender = user_response.value().user_info();

    std::string file_content;
    MessageRecord message =
        ToMessageRecord(request, NewId(), user_id.value().value(),
                        UnixTimeSeconds(), &file_content);

    std::string queue_payload;
    ToProtoMessage(message, FromProtoUser(sender), file_content)
        .SerializeToString(&queue_payload);

    zchat::GetChatSessionMemberIdsReq members_request;
    members_request.set_request_id(request.request_id());
    members_request.set_chat_session_id(request.chat_session_id());

    auto publish_fut = std::async(
        std::launch::async, [&]() { return queue_.Publish(queue_payload); });
    auto members_fut = std::async(std::launch::async, [&]() {
        return clients_.GetChatSessionMemberIds(members_request);
    });

    const auto published = publish_fut.get();
    if (!published.ok()) {
        return MakeErrorResponse<zchat::NewMessageRsp>(request.request_id(),
                                                       published.error());
    }

    auto members_response = members_fut.get();
    if (members_response.ok() && members_response.value().success()) {
        zchat::NotifyMessage notify;
        notify.set_notify_type(zchat::CHAT_MESSAGE_NOTIFY);
        *notify.mutable_new_message_info()->mutable_message_info() =
            ToProtoMessage(message, FromProtoUser(sender), file_content);
        std::string payload;
        notify.SerializeToString(&payload);

        std::vector<std::string> targets;
        for (const auto &member_id : members_response.value().member_id()) {
            if (member_id != user_id.value().value()) {
                targets.push_back(member_id);
            }
        }

        int notify_total = static_cast<int>(targets.size());
        int notify_failed = 0;
        if (!targets.empty()) {
            auto outcome = notifier_.PublishBatch(targets, payload);
            if (outcome.ok()) {
                notify_failed = static_cast<int>(outcome.value().failed.size());
                for (const auto &failed_id : outcome.value().failed) {
                    ZCHAT_LOG_WARN("notify publish failed request={} member={}",
                                   request.request_id(), failed_id);
                }
            } else {
                notify_failed = notify_total;
                ZCHAT_LOG_WARN(
                    "notify publish batch failed request={} error={}",
                    request.request_id(), FormatErrorForLog(outcome.error()));
            }
        }

        ZCHAT_LOG_INFO("new message accepted request={} message={} chat={} "
                       "sender={} targets={} notify_total={} notify_failed={}",
                       request.request_id(), message.message_id,
                       request.chat_session_id(), user_id.value().value(),
                       targets.size(), notify_total, notify_failed);
    } else {
        ZCHAT_LOG_WARN("new message notify skipped request={} members_ok={}",
                       request.request_id(), members_response.ok());
    }

    zchat::NewMessageRsp response;
    response.set_request_id(request.request_id());
    response.set_success(true);
    response.set_errmsg("");
    return response;
}

} // namespace zchat