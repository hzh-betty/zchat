#include "gateway/gateway_builder.h"

#include <chrono>

#include <drogon/utils/coroutine.h>

#include "common/runtime.h"
#include "gateway/websocket_controller.h"

namespace zchat {

GatewayBuilder::GatewayBuilder(const AppConfig &config) : config_(config) {}

int GatewayBuilder::Start() {
    context_ = std::make_shared<GatewayContext>(config_);
    controller_ = std::make_unique<GatewayController>(context_);
    controller_->RegisterRoutes();
    ZchatWebSocketController::SetContext(context_);
    drogon::app().getLoop()->runEvery(
        std::chrono::seconds(120), [context = context_]() {
            auto &connections = context->connections();
            auto &sessions = context->sessions();
            connections.ForEachBoundUser(
                [&sessions](const std::string &user_id) {
                    [](SessionStore &sessions,
                       std::string uid) -> drogon::AsyncTask {
                        co_await sessions.RefreshOnlineCoro(uid);
                        co_return;
                    }(sessions, user_id);
                });
        });
    return RunDrogonGateway("zchat_gateway", config_.log, config_.server);
}

} // namespace zchat
