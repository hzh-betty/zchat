#include "transmite/transmite_service.h"

#include <algorithm>
#include <string>
#include <vector>

#include "common/common_errors.h"
#include "common/error_response.h"
#include "common/logger.h"
#include "common/proto_mapper.h"
#include "common/resource_limits.h"
#include "common/uuid.h"
#include "notify.pb.h"

namespace zchat {

TransmiteService::TransmiteService(MessageQueuePublisher &queue,
                                   SessionStore &sessions,
                                   NotifyPublisher &notifier,
                                   ServiceClients &clients)
    : queue_(queue), sessions_(sessions), notifier_(notifier),
      clients_(clients) {}

drogon::Task<zchat::NewMessageRsp>
TransmiteService::NewMessageCoro(const zchat::NewMessageReq &request) {
    if (request.ByteSizeLong() > resource_limits::kFileBytes + 128 * 1024) {
        co_return MakeErrorResponse<zchat::NewMessageRsp>(
            request.request_id(),
            AppError::WithCode(ErrorCode::kInvalidArgument,
                               "message is too large"));
    }
    if (request.message().message_type() == zchat::STRING &&
        request.message().string_message().content().size() >
            resource_limits::kTextBytes) {
        co_return MakeErrorResponse<zchat::NewMessageRsp>(
            request.request_id(),
            AppError::WithCode(ErrorCode::kInvalidArgument,
                               "text message is too large"));
    }
    const auto user_id = co_await sessions_.GetUserIdCoro(request.session_id());
    if (!user_id.ok()) {
        co_return MakeErrorResponse<zchat::NewMessageRsp>(request.request_id(),
                                                          user_id.error());
    }
    if (!user_id.value().has_value()) {
        co_return MakeErrorResponse<zchat::NewMessageRsp>(
            request.request_id(), common_errors::SessionExpired());
    }

    zchat::GetUserInfoReq user_request;
    user_request.set_session_id(request.session_id());
    user_request.set_user_id(user_id.value().value());
    auto user_response = co_await clients_.GetUserCoro(user_request);
    if (!user_response.ok() || !user_response.value().success()) {
        AppError error = common_errors::InternalServiceError();
        if (!user_response.ok()) {
            error = user_response.error();
        }
        co_return MakeErrorResponse<zchat::NewMessageRsp>(request.request_id(),
                                                          error);
    }
    const zchat::UserInfo &sender = user_response.value().user_info();

    zchat::GetChatSessionMemberIdsReq members_check;
    members_check.set_request_id(request.request_id());
    members_check.set_chat_session_id(request.chat_session_id());
    auto members_check_rsp =
        co_await clients_.GetChatSessionMemberIdsCoro(members_check);
    if (!members_check_rsp.ok() || !members_check_rsp.value().success()) {
        co_return MakeErrorResponse<zchat::NewMessageRsp>(
            request.request_id(), common_errors::InternalServiceError());
    }
    bool is_member = false;
    for (const auto &mid : members_check_rsp.value().member_id()) {
        if (mid == user_id.value().value()) {
            is_member = true;
            break;
        }
    }
    if (!is_member) {
        co_return MakeErrorResponse<zchat::NewMessageRsp>(
            request.request_id(),
            AppError::WithCode(ErrorCode::kForbidden,
                               "sender is not a member of this session"));
    }

    std::string message_id = NewId();
    if (request.has_message_id()) {
        const auto &candidate = request.message_id();
        if (candidate.size() != 32 ||
            !std::all_of(candidate.begin(), candidate.end(), [](char c) {
                return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                       (c >= 'A' && c <= 'F');
            })) {
            co_return MakeErrorResponse<zchat::NewMessageRsp>(
                request.request_id(),
                AppError::WithCode(ErrorCode::kInvalidArgument,
                                   "invalid message id"));
        }
        message_id = candidate;
    }
    std::string file_content;
    MessageRecord message =
        ToMessageRecord(request, message_id, user_id.value().value(),
                        UnixTimeSeconds(), &file_content);

    std::string queue_payload;
    ToProtoMessage(message, FromProtoUser(sender), file_content, "")
        .SerializeToString(&queue_payload);

    auto published = queue_.Publish(queue_payload);
    if (!published.ok()) {
        ZCHAT_LOG_ERROR("message queue publish failed request={} error={}",
                        request.request_id(), published.error().message);
        co_return MakeErrorResponse<zchat::NewMessageRsp>(
            request.request_id(),
            common_errors::InternalServiceError().WithDetail(
                "message persist failed"));
    }

    if (members_check_rsp.ok() && members_check_rsp.value().success()) {
        zchat::NotifyMessage notify;
        notify.set_notify_type(zchat::CHAT_MESSAGE_NOTIFY);
        *notify.mutable_new_message_info()->mutable_message_info() =
            ToProtoMessage(message, FromProtoUser(sender), file_content, "");
        std::string payload;
        notify.SerializeToString(&payload);

        std::vector<std::string> targets;
        for (const auto &member_id : members_check_rsp.value().member_id()) {
            if (member_id != user_id.value().value()) {
                targets.push_back(member_id);
            }
        }

        if (!targets.empty()) {
            auto outcome =
                co_await notifier_.PublishBatchCoro(targets, payload);
            if (outcome.ok()) {
                for (const auto &failed_id : outcome.value().failed) {
                    ZCHAT_LOG_WARN("notify publish failed request={} member={}",
                                   request.request_id(), failed_id);
                }
            } else {
                ZCHAT_LOG_WARN(
                    "notify publish batch failed request={} error={}",
                    request.request_id(), FormatErrorForLog(outcome.error()));
            }
        }

        ZCHAT_LOG_INFO("new message accepted request={} message={} chat={} "
                       "sender={} targets={}",
                       request.request_id(), message.message_id,
                       request.chat_session_id(), user_id.value().value(),
                       targets.size());
    } else {
        ZCHAT_LOG_WARN("new message notify skipped request={} members_ok={}",
                       request.request_id(), members_check_rsp.ok());
    }

    zchat::NewMessageRsp response;
    response.set_request_id(request.request_id());
    response.set_success(true);
    response.set_errmsg("");
    response.set_message_id(message.message_id);
    co_return response;
}

} // namespace zchat
