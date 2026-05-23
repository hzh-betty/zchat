#include "speech/speech_service.h"

#include <utility>

#include "common/logger.h"

namespace zchat {

SpeechApplicationService::SpeechApplicationService(SpeechRecognizer &recognizer)
    : recognizer_(recognizer) {}

zchat::SpeechRecognitionRsp SpeechApplicationService::Recognize(
    const zchat::SpeechRecognitionReq &request) {
    ZCHAT_LOG_INFO("SpeechService::Recognize request_id={}", request.request_id());
    zchat::SpeechRecognitionRsp response;
    response.set_request_id(request.request_id());
    auto result = recognizer_.Recognize(request.speech_content());
    response.set_success(result.ok());
    response.set_errmsg(result.ok() ? "" : result.error().message);
    response.set_recognition_result(result.ok() ? result.value() : "");
    if (!result.ok()) {
        ZCHAT_LOG_ERROR("SpeechService::Recognize failed: request_id={} err={}", request.request_id(), result.error().message);
    } else {
        ZCHAT_LOG_INFO("SpeechService::Recognize success: request_id={}", request.request_id());
    }
    return response;
}

} // namespace zchat
