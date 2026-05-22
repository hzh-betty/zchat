#include "speech/speech_grpc_service.h"

#include <utility>

namespace zchat {

SpeechGrpcService::SpeechGrpcService(
    std::shared_ptr<SpeechApplicationService> service)
    : service_(std::move(service)) {}

grpc::Status
SpeechGrpcService::SpeechRecognition(grpc::ServerContext *,
                                     const zchat::SpeechRecognitionReq *request,
                                     zchat::SpeechRecognitionRsp *response) {
    *response = service_->Recognize(*request);
    return grpc::Status::OK;
}

} // namespace zchat
