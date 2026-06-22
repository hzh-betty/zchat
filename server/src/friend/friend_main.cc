#include <cstdlib>

#include <memory>

#include "common/config.h"
#include "common/logger.h"
#include "common/runtime.h"
#include "friend/friend_context.h"

int main(int argc, char *argv[]) {
    try {
        const zchat::AppConfig config = zchat::LoadConfig(
            zchat::ConfigPath(argc, argv, "server/config/friend.json"));
        zchat::FriendContext context(config);
        return zchat::RunGrpcServer("zchat_friend_service", config.log,
                                    config.services.friend_service,
                                    &context.grpc_service(), config.grpc,
                                    &config.etcd, "friend_service");
    } catch (const std::exception &e) {
        ZCHAT_LOG_FATAL("friend_service startup failed: {}", e.what());
        return EXIT_FAILURE;
    }
}
