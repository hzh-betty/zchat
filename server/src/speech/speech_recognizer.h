#ifndef ZCHAT_SERVER_SRC_SPEECH_SPEECH_RECOGNIZER_H_
#define ZCHAT_SERVER_SRC_SPEECH_SPEECH_RECOGNIZER_H_

#include "common/noncopyable.h"

#include <string>

#include <drogon/utils/coroutine.h>

#include "common/result.h"

namespace zchat {

class SpeechRecognizer : public NonCopyable {
  public:
    SpeechRecognizer() = default;

    virtual ~SpeechRecognizer() = default;

    virtual drogon::Task<Result<std::string>>
    RecognizeCoro(const std::string &speech_data) = 0;
};

class ConfiguredSpeechRecognizer final : public SpeechRecognizer {
  public:
    ConfiguredSpeechRecognizer() = default;
    ~ConfiguredSpeechRecognizer() override = default;

    drogon::Task<Result<std::string>>
    RecognizeCoro(const std::string &speech_data) override;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_SPEECH_SPEECH_RECOGNIZER_H_
