#include "speech/speech_context.h"

namespace zchat {

SpeechContext::SpeechContext(const AppConfig &config)
    : config_(config),
      recognizer_(config.speech.enabled, config.speech.placeholder_result),
      speech_service_(std::make_shared<SpeechApplicationService>(recognizer_)),
      grpc_service_(speech_service_) {}

} // namespace zchat
