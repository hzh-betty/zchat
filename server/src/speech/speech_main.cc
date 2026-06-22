#include <memory>

#include "common/config.h"
#include "common/runtime.h"
#include "speech/speech_context.h"

int main(int argc, char *argv[]) {
    const zchat::AppConfig config = zchat::LoadConfig(
        zchat::ConfigPath(argc, argv, "server/config/speech.json"));
    zchat::SpeechContext context(config);
    return zchat::RunGrpcServer("zchat_speech_service", config.log,
                                config.services.speech, &context.grpc_service(),
                                config.grpc, &config.etcd, "speech_service");
}
