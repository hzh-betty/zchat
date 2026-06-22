#include <cstdlib>

#include <memory>

#include "common/config.h"
#include "common/logger.h"
#include "common/runtime.h"
#include "file/file_context.h"

int main(int argc, char *argv[]) {
    try {
        const zchat::AppConfig config = zchat::LoadConfig(
            zchat::ConfigPath(argc, argv, "server/config/file.json"));
        zchat::FileContext context(config);
        return zchat::RunGrpcServer(
            "zchat_file_service", config.log, config.services.file,
            &context.grpc_service(), config.grpc, &config.etcd, "file_service");
    } catch (const std::exception &e) {
        ZCHAT_LOG_FATAL("file_service startup failed: {}", e.what());
        return EXIT_FAILURE;
    }
}
