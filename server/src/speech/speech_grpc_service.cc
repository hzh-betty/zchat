#include "speech/speech_grpc_service.h"

#include <utility>

#include "common/error_response.h"

namespace zchat {

SpeechGrpcService::SpeechGrpcService(
    std::shared_ptr<SpeechApplicationService> service)
    : service_(std::move(service)) {}

grpc::Status
SpeechGrpcService::SpeechRecognition(grpc::ServerContext *,
                                     const zchat::SpeechRecognitionReq *request,
                                     zchat::SpeechRecognitionRsp *response) {
    *response = service_->Recognize(*request);
    LogBoundaryResponseError("SpeechService", "SpeechRecognition",
                             request->request_id(), *response);
    return grpc::Status::OK;
}

} // namespace zchat
