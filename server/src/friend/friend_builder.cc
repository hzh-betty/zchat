#include "friend/friend_builder.h"

#include "common/runtime.h"

namespace zchat {

FriendBuilder::FriendBuilder(const AppConfig &config) : config_(config) {}

int FriendBuilder::Start() {
    context_ = std::make_unique<FriendContext>(config_);
    return RunGrpcServer("zchat_friend_service", config_.log,
                         config_.services.friend_service,
                         &context_->grpc_service(), config_.grpc, &config_.etcd,
                         "friend_service");
}

} // namespace zchat
