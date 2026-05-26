#include "transmite/transmite_grpc_service.h"

#include <utility>

#include "common/error_response.h"

namespace zchat {

TransmiteGrpcService::TransmiteGrpcService(
    std::shared_ptr<TransmiteService> service)
    : service_(std::move(service)) {}

grpc::Status
TransmiteGrpcService::GetTransmitTarget(grpc::ServerContext *,
                                        const zchat::NewMessageReq *request,
                                        zchat::GetTransmitTargetRsp *response) {
    const zchat::NewMessageRsp result = service_->NewMessage(*request);
    response->set_request_id(result.request_id());
    response->set_success(result.success());
    response->set_errmsg(result.errmsg());
    LogBoundaryResponseError("MsgTransmitService", "GetTransmitTarget",
                             request->request_id(), *response);
    return grpc::Status::OK;
}

} // namespace zchat
