#ifndef ZCHAT_SERVER_SRC_SPEECH_SPEECH_ERRORS_H_
#define ZCHAT_SERVER_SRC_SPEECH_SPEECH_ERRORS_H_

#include "common/result.h"

namespace zchat::speech_errors {

inline AppError RecognitionFailed(std::string message) {
    return AppError::WithCode(ErrorCode::kSpeechRecognitionFailed,
                              std::move(message));
}

inline AppError RecognizerNotConfigured() {
    return AppError::WithCode(ErrorCode::kExternalServiceError,
                              "speech recognizer is not configured");
}

} // namespace zchat::speech_errors

#endif // ZCHAT_SERVER_SRC_SPEECH_SPEECH_ERRORS_H_
