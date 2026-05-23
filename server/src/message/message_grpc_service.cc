#include "common/logger.h"
#include "message/message_grpc_service.h"

#include <utility>

namespace zchat {

MessageGrpcService::MessageGrpcService(std::shared_ptr<MessageService> service)
    : service_(std::move(service)) {}

grpc::Status
MessageGrpcService::GetHistoryMsg(grpc::ServerContext *,
                                   const zchat::GetHistoryMsgReq *request,
                                   zchat::GetHistoryMsgRsp *response) {
    ZCHAT_LOG_INFO("MsgStorageService::GetHistoryMsg request_id={}", request->request_id());
    *response = service_->GetHistory(*request);
    if (!response->success()) {
        ZCHAT_LOG_WARN("MsgStorageService::GetHistoryMsg failed: request_id={} errmsg={}", request->request_id(), response->errmsg());
    }
    return grpc::Status::OK;
}

grpc::Status
MessageGrpcService::GetRecentMsg(grpc::ServerContext *,
                                  const zchat::GetRecentMsgReq *request,
                                  zchat::GetRecentMsgRsp *response) {
    ZCHAT_LOG_INFO("MsgStorageService::GetRecentMsg request_id={}", request->request_id());
    *response = service_->GetRecent(*request);
    if (!response->success()) {
        ZCHAT_LOG_WARN("MsgStorageService::GetRecentMsg failed: request_id={} errmsg={}", request->request_id(), response->errmsg());
    }
    return grpc::Status::OK;
}

grpc::Status MessageGrpcService::MsgSearch(grpc::ServerContext *,
                                           const zchat::MsgSearchReq *request,
                                           zchat::MsgSearchRsp *response) {
    ZCHAT_LOG_INFO("MsgStorageService::MsgSearch request_id={}", request->request_id());
    *response = service_->Search(*request);
    if (!response->success()) {
        ZCHAT_LOG_WARN("MsgStorageService::MsgSearch failed: request_id={} errmsg={}", request->request_id(), response->errmsg());
    }
    return grpc::Status::OK;
}

} // namespace zchat
