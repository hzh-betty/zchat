#include "user/user_builder.h"

#include "common/runtime.h"

namespace zchat {

UserBuilder::UserBuilder(const AppConfig &config) : config_(config) {}

int UserBuilder::Start() {
    context_ = std::make_unique<UserContext>(config_);
    return RunGrpcServer("zchat_user_service", config_.services.user,
                         &context_->grpc_service());
}

} // namespace zchat
