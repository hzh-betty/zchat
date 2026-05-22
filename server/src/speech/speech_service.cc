#include "speech/speech_service.h"

#include <utility>

namespace zchat {

SpeechApplicationService::SpeechApplicationService(SpeechRecognizer &recognizer)
    : recognizer_(recognizer) {}

zchat::SpeechRecognitionRsp SpeechApplicationService::Recognize(
    const zchat::SpeechRecognitionReq &request) {
    zchat::SpeechRecognitionRsp response;
    response.set_request_id(request.request_id());
    auto result = recognizer_.Recognize(request.speech_content());
    response.set_success(result.ok());
    response.set_errmsg(result.ok() ? "" : result.error().message);
    response.set_recognition_result(result.ok() ? result.value() : "");
    return response;
}

} // namespace zchat
