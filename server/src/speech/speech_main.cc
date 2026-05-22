#include <memory>

#include "common/config.h"
#include "common/runtime.h"
#include "speech/speech_builder.h"

int main(int argc, char *argv[]) {
    const zchat::AppConfig config =
        zchat::LoadConfig(zchat::ConfigPath(argc, argv));
    auto server = std::make_unique<zchat::SpeechBuilder>(config);
    return server->Start();
}
