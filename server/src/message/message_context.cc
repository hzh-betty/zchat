#include "message/message_context.h"

#include "common/runtime.h"

namespace zchat {

MessageContext::MessageContext(const AppConfig &config)
    : config_(config), db_(MakeDbClient(config.mysql)),
      message_repository_(db_), user_repository_(db_), file_repository_(db_),
      search_index_(config.elasticsearch),
      message_service_(std::make_shared<MessageService>(
          message_repository_, user_repository_, file_repository_,
          search_index_)),
      grpc_service_(message_service_) {}

} // namespace zchat
