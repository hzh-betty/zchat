#include <memory>

#include "common/config.h"
#include "common/runtime.h"
#include "gateway/gateway_builder.h"

int main(int argc, char *argv[]) {
    const zchat::AppConfig config = zchat::LoadConfig(
        zchat::ConfigPath(argc, argv, "server/config/gateway.json"));
    auto server = std::make_unique<zchat::GatewayBuilder>(config);
    return server->Start();
}
