#include "message/message_grpc_service.h"

#include <utility>

#include "common/error_response.h"
#include "common/logger.h"

namespace zchat {

MessageGrpcService::MessageGrpcService(std::shared_ptr<MessageService> service)
    : service_(std::move(service)) {}

grpc::Status
MessageGrpcService::GetHistoryMsg(grpc::ServerContext *,
                                   const zchat::GetHistoryMsgReq *request,
                                   zchat::GetHistoryMsgRsp *response) {
    ZCHAT_LOG_INFO("MsgStorageService::GetHistoryMsg request_id={}", request->request_id());
    *response = service_->GetHistory(*request);
    LogBoundaryResponseError("MsgStorageService", "GetHistoryMsg",
                             request->request_id(), *response);
    return grpc::Status::OK;
}

grpc::Status
MessageGrpcService::GetRecentMsg(grpc::ServerContext *,
                                  const zchat::GetRecentMsgReq *request,
                                  zchat::GetRecentMsgRsp *response) {
    ZCHAT_LOG_INFO("MsgStorageService::GetRecentMsg request_id={}", request->request_id());
    *response = service_->GetRecent(*request);
    LogBoundaryResponseError("MsgStorageService", "GetRecentMsg",
                             request->request_id(), *response);
    return grpc::Status::OK;
}

grpc::Status MessageGrpcService::MsgSearch(grpc::ServerContext *,
                                           const zchat::MsgSearchReq *request,
                                           zchat::MsgSearchRsp *response) {
    ZCHAT_LOG_INFO("MsgStorageService::MsgSearch request_id={}", request->request_id());
    *response = service_->Search(*request);
    LogBoundaryResponseError("MsgStorageService", "MsgSearch",
                             request->request_id(), *response);
    return grpc::Status::OK;
}

} // namespace zchat
