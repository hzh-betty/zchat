#include "gateway/gateway_context.h"

#include "common/runtime.h"

namespace zchat {

GatewayContext::GatewayContext(const AppConfig &config)
    : config_(config), redis_(MakeRedisClient(config.redis)), sessions_(redis_),
      grpc_clients_(config_), notify_subscriber_(redis_, connections_) {
    notify_subscriber_.Start();
}

} // namespace zchat
