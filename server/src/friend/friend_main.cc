#include <memory>

#include "common/config.h"
#include "common/runtime.h"
#include "friend/friend_builder.h"

int main(int argc, char *argv[]) {
    const zchat::AppConfig config =
        zchat::LoadConfig(
            zchat::ConfigPath(argc, argv, "server/config/friend.json"));
    auto server = std::make_unique<zchat::FriendBuilder>(config);
    return server->Start();
}
