#include "message/message_builder.h"

#include "common/runtime.h"

namespace zchat {

MessageBuilder::MessageBuilder(const AppConfig &config) : config_(config) {}

int MessageBuilder::Start() {
    context_ = std::make_unique<MessageContext>(config_);
    return RunGrpcServer("zchat_message_service", config_.services.message,
                         &context_->grpc_service());
}

} // namespace zchat
