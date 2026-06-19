#include "user/user_builder.h"

#include "common/runtime.h"

namespace zchat {

UserBuilder::UserBuilder(const AppConfig &config) : config_(config) {}

int UserBuilder::Start() {
    context_ = std::make_unique<UserContext>(config_);
    return RunGrpcServer("zchat_user_service", config_.log,
                         config_.services.user, &context_->grpc_service(),
                         config_.grpc, &config_.etcd, "user_service");
}

} // namespace zchat
