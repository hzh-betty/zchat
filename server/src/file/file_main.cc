#include <memory>

#include "common/config.h"
#include "common/runtime.h"
#include "file/file_builder.h"

int main(int argc, char *argv[]) {
    const zchat::AppConfig config =
        zchat::LoadConfig(
            zchat::ConfigPath(argc, argv, "server/config/file.json"));
    auto server = std::make_unique<zchat::FileBuilder>(config);
    return server->Start();
}
