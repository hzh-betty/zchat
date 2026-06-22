#include <cstdlib>

#include <memory>

#include "common/config.h"
#include "common/logger.h"
#include "common/runtime.h"
#include "user/user_context.h"

int main(int argc, char *argv[]) {
    try {
        const zchat::AppConfig config = zchat::LoadConfig(
            zchat::ConfigPath(argc, argv, "server/config/user.json"));
        zchat::UserContext context(config);
        return zchat::RunGrpcServer(
            "zchat_user_service", config.log, config.services.user,
            &context.grpc_service(), config.grpc, &config.etcd, "user_service");
    } catch (const std::exception &e) {
        ZCHAT_LOG_FATAL("user_service startup failed: {}", e.what());
        return EXIT_FAILURE;
    }
}
