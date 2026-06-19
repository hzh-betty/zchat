#include <memory>

#include "common/config.h"
#include "common/runtime.h"
#include "message/message_builder.h"

int main(int argc, char *argv[]) {
    try {
        const zchat::AppConfig config = zchat::LoadConfig(
            zchat::ConfigPath(argc, argv, "server/config/message.json"));
        auto server = std::make_unique<zchat::MessageBuilder>(config);
        return server->Start();
    } catch (const std::exception &e) {
        ZCHAT_LOG_FATAL("message_service startup failed: {}", e.what());
        return EXIT_FAILURE;
    }
}
