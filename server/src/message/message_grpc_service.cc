#include "message/message_grpc_service.h"

#include <utility>

#include "common/error_response.h"
#include "common/grpc_callback.h"
#include "common/logger.h"

namespace zchat {

MessageGrpcService::MessageGrpcService(std::shared_ptr<MessageService> service)
    : service_(std::move(service)) {}

#define ZCHAT_MSG_RPC(method_name, req_type, rsp_type, coro_name)              \
    grpc::ServerUnaryReactor *MessageGrpcService::method_name(                 \
        grpc::CallbackServerContext *, const zchat::req_type *request,         \
        zchat::rsp_type *response) {                                           \
        ZCHAT_LOG_INFO("MsgStorageService::" #method_name " request_id={}",    \
                       request->request_id());                                 \
        return new CoroUnaryReactor<zchat::rsp_type>(                          \
            [this, req = *request]() -> drogon::Task<zchat::rsp_type> {        \
                co_return co_await service_->coro_name(req);                   \
            },                                                                 \
            response, "MsgStorageService", #method_name,                       \
            request->request_id());                                            \
    }

ZCHAT_MSG_RPC(GetHistoryMsg, GetHistoryMsgReq, GetHistoryMsgRsp, GetHistoryCoro)
ZCHAT_MSG_RPC(GetRecentMsg, GetRecentMsgReq, GetRecentMsgRsp, GetRecentCoro)
ZCHAT_MSG_RPC(GetMultiRecentMsg, GetMultiRecentMsgReq, GetMultiRecentMsgRsp,
              GetMultiRecentCoro)
ZCHAT_MSG_RPC(MsgSearch, MsgSearchReq, MsgSearchRsp, SearchCoro)

} // namespace zchat
