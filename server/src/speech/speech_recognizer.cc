#include "speech/speech_recognizer.h"

namespace zchat {

Result<std::string> ConfiguredSpeechRecognizer::Recognize(const std::string &) {
    return Result<std::string>::Fail(AppError::WithCode(
        ErrorCode::kExternalServiceError, "speech recognizer is not configured"));
}

} // namespace zchat
