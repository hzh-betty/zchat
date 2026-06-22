#include <memory>

#include "common/config.h"
#include "common/runtime.h"
#include "transmite/transmite_context.h"

int main(int argc, char *argv[]) {
    const zchat::AppConfig config = zchat::LoadConfig(
        zchat::ConfigPath(argc, argv, "server/config/transmite.json"));
    zchat::TransmiteContext context(config);
    return zchat::RunGrpcServer("zchat_transmite_service", config.log,
                                config.services.transmite,
                                &context.grpc_service(), config.grpc,
                                &config.etcd, "transmite_service");
}
