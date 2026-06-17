#include "gateway/gateway_controller.h"

#include <string>
#include <utility>

#include "common/common_errors.h"
#include "common/logger.h"
#include "common/protobuf_http.h"
#include "gateway/route_table.h"

namespace zchat {

GatewayController::GatewayController(std::shared_ptr<GatewayContext> context)
    : context_(std::move(context)) {}

void GatewayController::RegisterRoutes() {
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
        [this, path, service_name](
            const drogon::HttpRequestPtr &request,
            std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
            ZCHAT_LOG_DEBUG("forward http path={} service={} body={}B", path,
                            service_name, request->body().size());
            const auto *route = FindRoute(path);
            if (route == nullptr) {
                ZCHAT_LOG_WARN("unknown service path: {}", path);
                callback(TextResponse(
                    FormatErrorForClient(common_errors::UnknownServicePath())));
                return;
            }
            route->handle(&context_->sessions(), context_->grpc_clients(),
                          std::string(request->body()), std::move(callback));
        },
        {drogon::Post});
}

} // namespace zchat
