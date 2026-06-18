#include "speech/speech_recognizer.h"

#include "speech/speech_errors.h"

namespace zchat {

drogon::Task<Result<std::string>>
ConfiguredSpeechRecognizer::RecognizeCoro(const std::string &) {
    co_return Result<std::string>::Fail(
        speech_errors::RecognizerNotConfigured());
}

} // namespace zchat
