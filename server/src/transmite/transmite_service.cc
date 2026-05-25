#include "transmite/transmite_service.h"

#include <string>

#include "common/logger.h"
#include "common/proto_mapper.h"
#include "common/uuid.h"
#include "notify.pb.h"

namespace zchat {
namespace {

zchat::NewMessageRsp ErrorResponse(const std::string &request_id,
                                   const std::string &message) {
    zchat::NewMessageRsp response;
    response.set_request_id(request_id);
    response.set_success(false);
    response.set_errmsg(message);
    return response;
}

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
        ZCHAT_LOG_WARN("new message rejected session={} error={}",
                       request.session_id(), user_id.error().message);
        return ErrorResponse(request.request_id(), user_id.error().message);
    }
    if (!user_id.value().has_value()) {
        ZCHAT_LOG_WARN("new message rejected session={} error=expired",
                       request.session_id());
        return ErrorResponse(request.request_id(), "登录会话已失效");
    }

    std::string file_content;
    MessageRecord message =
        ToMessageRecord(request, NewId(), user_id.value().value(),
                        UnixTimeSeconds(), &file_content);
    auto sender = users_.FindUserById(user_id.value().value());
    if (!sender.ok() || !sender.value().has_value()) {
        ZCHAT_LOG_ERROR("new message sender lookup failed request={} sender={}",
                        request.request_id(), user_id.value().value());
        return ErrorResponse(request.request_id(), "发送者信息不存在");
    }

    if (queue_.enabled()) {
        std::string queue_payload;
        ToProtoMessage(message, sender.value().value(), file_content)
            .SerializeToString(&queue_payload);
        const auto published = queue_.Publish(queue_payload);
        if (!published.ok()) {
            ZCHAT_LOG_ERROR("queue publish failed request={} message={} error={}",
                            request.request_id(), message.message_id,
                            published.error().message);
            return ErrorResponse(request.request_id(), published.error().message);
        }
    } else {
        if (!file_content.empty()) {
            const auto saved = repository_.PutFile(FileRecord{
                message.file_id, message.file_name, file_content.size(),
                file_content});
            if (!saved.ok()) {
                ZCHAT_LOG_ERROR("save message file failed request={} error={}",
                                request.request_id(), saved.error().message);
                return ErrorResponse(request.request_id(), saved.error().message);
            }
        }
        const auto inserted = repository_.InsertMessage(message);
        if (!inserted.ok()) {
            ZCHAT_LOG_ERROR("insert message failed request={} chat={} error={}",
                            request.request_id(), request.chat_session_id(),
                            inserted.error().message);
            return ErrorResponse(request.request_id(), inserted.error().message);
        }
        const auto indexed = search_index_.IndexMessage(message);
        if (!indexed.ok()) {
            ZCHAT_LOG_ERROR("index message failed request={} message={} error={}",
                            request.request_id(), message.message_id,
                            indexed.error().message);
            return ErrorResponse(request.request_id(), indexed.error().message);
        }
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
