#ifndef ZCHAT_SERVER_SRC_SPEECH_SPEECH_RECOGNIZER_H_
#define ZCHAT_SERVER_SRC_SPEECH_SPEECH_RECOGNIZER_H_

#include "common/noncopyable.h"

#include <string>

#include "common/result.h"

namespace zchat {

class SpeechRecognizer : public NonCopyable {
  public:
    SpeechRecognizer() = default;

    virtual ~SpeechRecognizer() = default;

    virtual Result<std::string> Recognize(const std::string &speech_data) = 0;
};

class ConfiguredSpeechRecognizer final : public SpeechRecognizer {
  public:
    ConfiguredSpeechRecognizer(bool enabled, std::string placeholder_result);
    ~ConfiguredSpeechRecognizer() override = default;

    Result<std::string> Recognize(const std::string &speech_data) override;

  private:
    bool enabled_;
    std::string placeholder_result_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_SPEECH_SPEECH_RECOGNIZER_H_
