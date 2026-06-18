#include "transmite/transmite_grpc_service.h"

#include <utility>

#include "common/error_response.h"
#include "common/grpc_callback.h"
#include "common/logger.h"

namespace zchat {

TransmiteGrpcService::TransmiteGrpcService(
    std::shared_ptr<TransmiteService> service)
    : service_(std::move(service)) {}

grpc::ServerUnaryReactor *
TransmiteGrpcService::GetTransmitTarget(grpc::CallbackServerContext *,
                                        const zchat::NewMessageReq *request,
                                        zchat::GetTransmitTargetRsp *response) {
    ZCHAT_LOG_INFO("MsgTransmitService::GetTransmitTarget request_id={}",
                   request->request_id());
    return new CoroUnaryReactor<zchat::GetTransmitTargetRsp>(
        [this, req = *request]() -> drogon::Task<zchat::GetTransmitTargetRsp> {
            auto result = co_await service_->NewMessageCoro(req);
            zchat::GetTransmitTargetRsp rsp;
            rsp.set_request_id(result.request_id());
            rsp.set_success(result.success());
            rsp.set_errmsg(result.errmsg());
            co_return rsp;
        },
        response, "MsgTransmitService", "GetTransmitTarget",
        request->request_id());
}

} // namespace zchat
