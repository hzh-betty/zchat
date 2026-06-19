#include "gateway/gateway_controller.h"

#include <string>
#include <utility>

#include <drogon/utils/coroutine.h>

#include "common/common_errors.h"
#include "common/logger.h"
#include "common/protobuf_http.h"
#include "gateway/route_table.h"

namespace zchat {

GatewayController::GatewayController(std::shared_ptr<GatewayContext> context)
    : context_(std::move(context)) {}

void GatewayController::RegisterRoutes() {

    drogon::app().setClientMaxBodySize(64 * 1024 * 1024);

    drogon::app().registerHandler(
        "/ping",
        [](const drogon::HttpRequestPtr &,
           std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
            callback(TextResponse("pong"));
        },
        {drogon::Get});

    for (const auto &route : GetAllRoutes()) {
        RegisterForwardPost(route.path, route.service_name);
    }
}

void GatewayController::RegisterForwardPost(const std::string &path,
                                            const std::string &service_name) {
    drogon::app().registerHandler(
        path,
        [this, path](
            const drogon::HttpRequestPtr &request,
            std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
            ZCHAT_LOG_DEBUG("forward http path={} body={}B", path,
                            request->body().size());
            const auto *route = FindRoute(path);
            if (route == nullptr) {
                ZCHAT_LOG_WARN("unknown service path: {}", path);
                callback(TextResponse(
                    FormatErrorForClient(common_errors::UnknownServicePath())));
                return;
            }
            auto ctx = context_;
            auto handle = route->handle;
            auto body = std::string(request->body());
            [](std::shared_ptr<GatewayContext> ctx,
               std::function<drogon::Task<drogon::HttpResponsePtr>(
                   SessionStore *, GrpcServiceClients &, const std::string &)>
                   handle,
               std::string body,
               std::function<void(const drogon::HttpResponsePtr &)> callback)
                -> drogon::AsyncTask {
                try {
                    auto resp = co_await handle(&ctx->sessions(),
                                                ctx->grpc_clients(), body);
                    callback(resp);
                } catch (const std::exception &e) {
                    ZCHAT_LOG_ERROR("gateway coroutine exception: {}",
                                    e.what());
                    callback(TextResponse("internal error"));
                }
                co_return;
            }(ctx, std::move(handle), std::move(body), std::move(callback));
        },
        {drogon::Post});
}

} // namespace zchat
