#include <cstdlib>

#include <memory>

#include "common/config.h"
#include "common/logger.h"
#include "common/runtime.h"
#include "message/message_context.h"

int main(int argc, char *argv[]) {
    try {
        const zchat::AppConfig config = zchat::LoadConfig(
            zchat::ConfigPath(argc, argv, "server/config/message.json"));
        zchat::MessageContext context(config);
        return zchat::RunGrpcServer("zchat_message_service", config.log,
                                    config.services.message,
                                    &context.grpc_service(), config.grpc,
                                    &config.etcd, "message_service");
    } catch (const std::exception &e) {
        ZCHAT_LOG_FATAL("message_service startup failed: {}", e.what());
        return EXIT_FAILURE;
    }
}
