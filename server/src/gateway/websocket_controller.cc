#include "gateway/websocket_controller.h"

#include <chrono>
#include <utility>

#include <drogon/utils/coroutine.h>

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
    if (context->connections().IsBound(connection)) {
        ZCHAT_LOG_DEBUG("websocket message ignored: connection already bound");
        return;
    }
    zchat::ClientAuthenticationReq request;
    if (!request.ParseFromString(message)) {
        ZCHAT_LOG_WARN("websocket auth rejected: protobuf parse failed");
        connection->shutdown();
        return;
    }

    auto conn_ptr = connection;
    auto session_id = request.session_id();
    auto ctx = context;
    [](std::shared_ptr<GatewayContext> ctx,
       drogon::WebSocketConnectionPtr conn_ptr,
       std::string session_id) -> drogon::AsyncTask {
        auto user_id = co_await ctx->sessions().GetUserIdCoro(session_id);
        if (!user_id.ok() || !user_id.value().has_value()) {
            ZCHAT_LOG_WARN("websocket auth rejected: invalid session={}",
                           RedactToken(session_id));
            conn_ptr->shutdown();
            co_return;
        }
        ctx->connections().Bind(user_id.value().value(), session_id, conn_ptr);
        conn_ptr->setPingMessage("", std::chrono::seconds(60));
        co_await ctx->sessions().SetOnlineCoro(user_id.value().value());
        ZCHAT_LOG_INFO("websocket authenticated user={} session={}",
                       user_id.value().value(), RedactToken(session_id));
        co_return;
    }(ctx, conn_ptr, std::move(session_id));
}

void ZchatWebSocketController::handleNewConnection(
    const drogon::HttpRequestPtr &,
    const drogon::WebSocketConnectionPtr &connection) {
    ZCHAT_LOG_DEBUG("websocket connected");

    drogon::app().getLoop()->runAfter(10, [connection]() {
        auto ctx = ZchatWebSocketController::context_.lock();
        if (ctx == nullptr) {
            return;
        }
        if (!ctx->connections().IsBound(connection)) {
            ZCHAT_LOG_WARN("websocket unauthenticated timeout, closing");
            connection->shutdown();
        }
    });
}

void ZchatWebSocketController::handleConnectionClosed(
    const drogon::WebSocketConnectionPtr &connection) {
    auto context = context_.lock();
    if (context == nullptr) {
        ZCHAT_LOG_WARN("websocket closed but gateway context expired");
        return;
    }
    std::string user_id;
    std::string session_id;
    const bool removed_current =
        context->connections().Remove(connection, &user_id, &session_id);
    if (!user_id.empty() && removed_current) {
        auto ctx = context;
        auto uid = std::move(user_id);
        [](std::shared_ptr<GatewayContext> ctx,
           std::string uid) -> drogon::AsyncTask {
            co_await ctx->sessions().SetOfflineCoro(uid);
            ZCHAT_LOG_INFO("websocket closed user={}", uid);
            co_return;
        }(ctx, std::move(uid));
    } else if (!user_id.empty()) {
        ZCHAT_LOG_DEBUG("stale websocket closed user={}", user_id);
    }
}

} // namespace zchat
