#include "speech/speech_recognizer.h"

#include "speech/speech_errors.h"

namespace zchat {

Result<std::string> ConfiguredSpeechRecognizer::Recognize(const std::string &) {
    return Result<std::string>::Fail(speech_errors::RecognizerNotConfigured());
}

} // namespace zchat
