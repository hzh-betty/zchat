#ifndef ZCHAT_SERVER_SRC_GATEWAY_WEBSOCKET_CONTROLLER_H_
#define ZCHAT_SERVER_SRC_GATEWAY_WEBSOCKET_CONTROLLER_H_

#include <memory>

#include <drogon/WebSocketController.h>

#include "gateway/gateway_context.h"

namespace zchat {

class ZchatWebSocketController final
    : public drogon::WebSocketController<ZchatWebSocketController> {
  public:
    ZchatWebSocketController() = default;

    ~ZchatWebSocketController() override = default;

    static void SetContext(std::shared_ptr<GatewayContext> context);

    void handleNewMessage(const drogon::WebSocketConnectionPtr &connection,
                          std::string &&message,
                          const drogon::WebSocketMessageType &type) override;
    void handleNewConnection(
        const drogon::HttpRequestPtr &request,
        const drogon::WebSocketConnectionPtr &connection) override;
    void handleConnectionClosed(
        const drogon::WebSocketConnectionPtr &connection) override;

    WS_PATH_LIST_BEGIN
    WS_PATH_ADD("/ws");
    WS_PATH_LIST_END

  private:
    static std::weak_ptr<GatewayContext> context_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_GATEWAY_WEBSOCKET_CONTROLLER_H_
