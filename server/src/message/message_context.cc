#include "message/message_context.h"

#include "base.pb.h"
#include "common/result.h"
#include "common/runtime.h"
#include "message/message_errors.h"

namespace zchat {

MessageContext::MessageContext(const AppConfig &config)
    : config_(config), db_(MakeDbClient(config.mysql)),
      message_repository_(db_), clients_(config.etcd),
      search_index_(config.elasticsearch),
      message_service_(std::make_shared<MessageService>(
          message_repository_, clients_, search_index_)),
      queue_consumer_(config.rabbitmq,
                      [this](const std::string &payload) {
                          zchat::MessageInfo message;
                          if (!message.ParseFromString(payload)) {
                              return VoidResult::Fail(
                                  message_errors::QueuePayloadParseFailed());
                          }
                          return message_service_->StoreQueuedMessage(message);
                      }),
      grpc_service_(message_service_) {}

} // namespace zchat