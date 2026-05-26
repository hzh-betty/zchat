#include "message/message_context.h"

#include "common/runtime.h"
#include "base.pb.h"

namespace zchat {

MessageContext::MessageContext(const AppConfig &config)
    : config_(config), db_(MakeDbClient(config.mysql)),
      message_repository_(db_), user_repository_(db_), file_repository_(db_),
      friend_repository_(db_), search_index_(config.elasticsearch),
      message_service_(std::make_shared<MessageService>(
          message_repository_, user_repository_, file_repository_,
          friend_repository_, search_index_)),
      queue_consumer_(config.rabbitmq, [this](const std::string &payload) {
          zchat::MessageInfo message;
          if (!message.ParseFromString(payload)) {
              return VoidResult::Fail("RabbitMQ 消息反序列化失败");
          }
          return message_service_->StoreQueuedMessage(message);
      }),
      grpc_service_(message_service_) {}

} // namespace zchat
