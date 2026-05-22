#ifndef ZCHAT_SERVER_SRC_SPEECH_SPEECH_SERVICE_H_
#define ZCHAT_SERVER_SRC_SPEECH_SPEECH_SERVICE_H_

#include "common/noncopyable.h"

#include <string>

#include "speech.pb.h"
#include "speech/speech_recognizer.h"

namespace zchat {

class SpeechApplicationService : public NonCopyable {
  public:
    explicit SpeechApplicationService(SpeechRecognizer &recognizer);

    ~SpeechApplicationService() = default;

    zchat::SpeechRecognitionRsp
    Recognize(const zchat::SpeechRecognitionReq &request);

  private:
    SpeechRecognizer &recognizer_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_SPEECH_SPEECH_SERVICE_H_
