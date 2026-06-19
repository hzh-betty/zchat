#include "transmite/transmite_builder.h"

#include "common/runtime.h"

namespace zchat {

TransmiteBuilder::TransmiteBuilder(const AppConfig &config) : config_(config) {}

int TransmiteBuilder::Start() {
    context_ = std::make_unique<TransmiteContext>(config_);
    return RunGrpcServer("zchat_transmite_service", config_.log,
                         config_.services.transmite, &context_->grpc_service(),
                         config_.grpc, &config_.etcd, "transmite_service");
}

} // namespace zchat
