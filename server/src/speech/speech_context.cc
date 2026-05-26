#include "speech/speech_context.h"

#include "speech/baidu_speech_recognizer.h"

namespace zchat {

SpeechContext::SpeechContext(const AppConfig &config)
    : config_(config),
      recognizer_(std::make_unique<BaiduSpeechRecognizer>(config.speech)),
      speech_service_(
          std::make_shared<SpeechApplicationService>(*recognizer_)),
      grpc_service_(speech_service_) {}

} // namespace zchat
