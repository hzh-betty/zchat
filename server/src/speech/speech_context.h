#ifndef ZCHAT_SERVER_SRC_SPEECH_SPEECH_CONTEXT_H_
#define ZCHAT_SERVER_SRC_SPEECH_SPEECH_CONTEXT_H_

#include "common/noncopyable.h"

#include <memory>

#include "common/config.h"
#include "speech/speech_grpc_service.h"
#include "speech/speech_recognizer.h"
#include "speech/speech_service.h"

namespace zchat {

class SpeechContext : public NonCopyable {
  public:
    explicit SpeechContext(const AppConfig &config);

    ~SpeechContext() = default;

    SpeechGrpcService &grpc_service() { return grpc_service_; }

  private:
    AppConfig config_;
    ConfiguredSpeechRecognizer recognizer_;
    std::shared_ptr<SpeechApplicationService> speech_service_;
    SpeechGrpcService grpc_service_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_SPEECH_SPEECH_CONTEXT_H_
