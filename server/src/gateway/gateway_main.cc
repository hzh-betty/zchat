#include <chrono>
#include <cstdlib>
#include <memory>

#include <drogon/HttpAppFramework.h>
#include <drogon/utils/coroutine.h>

#include "common/config.h"
#include "common/logger.h"
#include "common/runtime.h"
#include "gateway/gateway_context.h"
#include "gateway/gateway_controller.h"
#include "gateway/websocket_controller.h"

int main(int argc, char *argv[]) {
    try {
        const zchat::AppConfig config = zchat::LoadConfig(
            zchat::ConfigPath(argc, argv, "server/config/gateway.json"));
        auto context = std::make_shared<zchat::GatewayContext>(config);
        zchat::GatewayController controller(context);
        controller.RegisterRoutes();
        zchat::ZchatWebSocketController::SetContext(context);
        drogon::app().getLoop()->runEvery(
            std::chrono::seconds(120), [context]() {
                auto &connections = context->connections();
                auto &sessions = context->sessions();
                connections.ForEachBoundUser(
                    [&sessions](const std::string &user_id) {
                        [](zchat::SessionStore &sessions,
                           std::string uid) -> drogon::AsyncTask {
                            co_await sessions.RefreshOnlineCoro(uid);
                            co_return;
                        }(sessions, user_id);
                    });
            });
        return zchat::RunDrogonGateway("zchat_gateway", config.log,
                                       config.server);
    } catch (const std::exception &e) {
        ZCHAT_LOG_FATAL("gateway startup failed: {}", e.what());
        return EXIT_FAILURE;
    }
}
