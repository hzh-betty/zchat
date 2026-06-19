#include "speech/speech_builder.h"

#include "common/runtime.h"

namespace zchat {

SpeechBuilder::SpeechBuilder(const AppConfig &config) : config_(config) {}

int SpeechBuilder::Start() {
    context_ = std::make_unique<SpeechContext>(config_);
    return RunGrpcServer("zchat_speech_service", config_.log,
                         config_.services.speech, &context_->grpc_service(),
                         config_.grpc, &config_.etcd, "speech_service");
}

} // namespace zchat
