#include <memory>

#include "common/config.h"
#include "common/runtime.h"
#include "friend/friend_builder.h"

int main(int argc, char *argv[]) {
    try {
        const zchat::AppConfig config = zchat::LoadConfig(
            zchat::ConfigPath(argc, argv, "server/config/friend.json"));
        auto server = std::make_unique<zchat::FriendBuilder>(config);
        return server->Start();
    } catch (const std::exception &e) {
        ZCHAT_LOG_FATAL("friend_service startup failed: {}", e.what());
        return EXIT_FAILURE;
    }
}
