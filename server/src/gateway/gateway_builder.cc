#include "gateway/gateway_builder.h"

#include "common/runtime.h"
#include "gateway/websocket_controller.h"

namespace zchat {

GatewayBuilder::GatewayBuilder(const AppConfig &config) : config_(config) {}

int GatewayBuilder::Start() {
    context_ = std::make_shared<GatewayContext>(config_);
    controller_ = std::make_unique<GatewayController>(context_);
    controller_->RegisterRoutes();
    ZchatWebSocketController::SetContext(context_);
    return RunDrogonGateway("zchat_gateway", config_.server.http_port,
                            config_.server.websocket_port);
}

} // namespace zchat
