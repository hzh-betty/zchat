#ifndef ZCHAT_SERVER_SRC_SPEECH_SPEECH_ERRORS_H_
#define ZCHAT_SERVER_SRC_SPEECH_SPEECH_ERRORS_H_

#include "common/result.h"

namespace zchat::speech_errors {

inline AppError RecognitionFailed() {
    return AppError::WithCode(ErrorCode::kSpeechRecognitionFailed,
                              "speech recognition failed");
}

} // namespace zchat::speech_errors

#endif // ZCHAT_SERVER_SRC_SPEECH_SPEECH_ERRORS_H_
