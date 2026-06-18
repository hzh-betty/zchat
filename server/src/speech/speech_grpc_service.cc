#include "speech/speech_grpc_service.h"

#include <utility>

#include "common/error_response.h"
#include "common/grpc_callback.h"
#include "common/logger.h"

namespace zchat {

SpeechGrpcService::SpeechGrpcService(
    std::shared_ptr<SpeechApplicationService> service)
    : service_(std::move(service)) {}

grpc::ServerUnaryReactor *
SpeechGrpcService::SpeechRecognition(grpc::CallbackServerContext *,
                                     const zchat::SpeechRecognitionReq *request,
                                     zchat::SpeechRecognitionRsp *response) {
    ZCHAT_LOG_INFO("SpeechService::SpeechRecognition request_id={}",
                   request->request_id());
    return new CoroUnaryReactor<zchat::SpeechRecognitionRsp>(
        [this, req = *request]() -> drogon::Task<zchat::SpeechRecognitionRsp> {
            co_return co_await service_->RecognizeCoro(req);
        },
        response, "SpeechService", "SpeechRecognition", request->request_id());
}

} // namespace zchat
