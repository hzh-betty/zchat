#include "speech/speech_service.h"

#include <utility>

#include "common/logger.h"
#include "common/result.h"

namespace zchat {

SpeechApplicationService::SpeechApplicationService(SpeechRecognizer &recognizer)
    : recognizer_(recognizer) {}

drogon::Task<zchat::SpeechRecognitionRsp>
SpeechApplicationService::RecognizeCoro(
    const zchat::SpeechRecognitionReq &request) {
    ZCHAT_LOG_INFO("SpeechService::Recognize request_id={}",
                   request.request_id());
    zchat::SpeechRecognitionRsp response;
    response.set_request_id(request.request_id());
    auto result = co_await recognizer_.RecognizeCoro(request.speech_content());
    response.set_success(result.ok());
    response.set_errmsg(result.ok() ? ""
                                    : FormatErrorForClient(result.error()));
    response.set_recognition_result(result.ok() ? result.value() : "");
    co_return response;
}

} // namespace zchat
