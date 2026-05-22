#include "gateway/websocket_controller.h"

#include "common/logger.h"
#include "gateway.pb.h"

namespace zchat {

std::weak_ptr<GatewayContext> ZchatWebSocketController::context_;

void ZchatWebSocketController::SetContext(
    std::shared_ptr<GatewayContext> context) {
    context_ = std::move(context);
}

void ZchatWebSocketController::handleNewMessage(
    const drogon::WebSocketConnectionPtr &connection, std::string &&message,
    const drogon::WebSocketMessageType &type) {
    if (type != drogon::WebSocketMessageType::Binary) {
        ZCHAT_LOG_WARN("websocket auth rejected: non-binary message");
        connection->shutdown();
        return;
    }
    auto context = context_.lock();
    if (context == nullptr) {
        ZCHAT_LOG_ERROR("websocket auth rejected: gateway context expired");
        connection->shutdown();
        return;
    }
    zchat::ClientAuthenticationReq request;
    if (!request.ParseFromString(message)) {
        ZCHAT_LOG_WARN("websocket auth rejected: protobuf parse failed");
        connection->shutdown();
        return;
    }
    auto user_id = context->sessions().GetUserId(request.session_id());
    if (!user_id.ok() || !user_id.value().has_value()) {
        ZCHAT_LOG_WARN("websocket auth rejected: invalid session={}",
                       request.session_id());
        connection->shutdown();
        return;
    }
    context->connections().Bind(user_id.value().value(), request.session_id(),
                                connection);
    context->sessions().SetOnline(user_id.value().value());
    ZCHAT_LOG_INFO("websocket authenticated user={} session={}",
                   user_id.value().value(), request.session_id());
}

void ZchatWebSocketController::handleNewConnection(
    const drogon::HttpRequestPtr &, const drogon::WebSocketConnectionPtr &) {
    ZCHAT_LOG_DEBUG("websocket connected");
}

void ZchatWebSocketController::handleConnectionClosed(
    const drogon::WebSocketConnectionPtr &connection) {
    auto context = context_.lock();
    if (context == nullptr) {
        return;
    }
    std::string user_id;
    std::string session_id;
    const bool removed_current =
        context->connections().Remove(connection, &user_id, &session_id);
    if (!user_id.empty() && removed_current) {
        context->sessions().SetOffline(user_id);
        ZCHAT_LOG_INFO("websocket closed user={} session={}", user_id,
                       session_id);
    } else if (!user_id.empty()) {
        ZCHAT_LOG_DEBUG("stale websocket closed user={} session={}", user_id,
                        session_id);
    }
}

} // namespace zchat
