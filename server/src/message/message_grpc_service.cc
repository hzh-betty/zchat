#include "message/message_grpc_service.h"

#include <utility>

namespace zchat {

MessageGrpcService::MessageGrpcService(std::shared_ptr<MessageService> service)
    : service_(std::move(service)) {}

grpc::Status
MessageGrpcService::GetHistoryMsg(grpc::ServerContext *,
                                  const zchat::GetHistoryMsgReq *request,
                                  zchat::GetHistoryMsgRsp *response) {
    *response = service_->GetHistory(*request);
    return grpc::Status::OK;
}

grpc::Status
MessageGrpcService::GetRecentMsg(grpc::ServerContext *,
                                 const zchat::GetRecentMsgReq *request,
                                 zchat::GetRecentMsgRsp *response) {
    *response = service_->GetRecent(*request);
    return grpc::Status::OK;
}

grpc::Status MessageGrpcService::MsgSearch(grpc::ServerContext *,
                                           const zchat::MsgSearchReq *request,
                                           zchat::MsgSearchRsp *response) {
    *response = service_->Search(*request);
    return grpc::Status::OK;
}

} // namespace zchat
