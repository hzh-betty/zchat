#include "friend/friend_grpc_service.h"

#include <utility>

#include "common/error_response.h"
#include "common/logger.h"

namespace zchat {

FriendGrpcService::FriendGrpcService(
    std::shared_ptr<FriendApplicationService> service)
    : service_(std::move(service)) {}

grpc::Status
FriendGrpcService::GetFriendList(grpc::ServerContext *,
                                 const zchat::GetFriendListReq *request,
                                 zchat::GetFriendListRsp *response) {
    ZCHAT_LOG_INFO("FriendService::GetFriendList request_id={}",
                   request->request_id());
    *response = service_->GetFriendList(*request);
    LogBoundaryResponseError("FriendService", "GetFriendList",
                             request->request_id(), *response);
    return grpc::Status::OK;
}

grpc::Status
FriendGrpcService::FriendRemove(grpc::ServerContext *,
                                const zchat::FriendRemoveReq *request,
                                zchat::FriendRemoveRsp *response) {
    ZCHAT_LOG_INFO("FriendService::FriendRemove request_id={}",
                   request->request_id());
    *response = service_->RemoveFriend(*request);
    LogBoundaryResponseError("FriendService", "FriendRemove",
                             request->request_id(), *response);
    return grpc::Status::OK;
}

grpc::Status FriendGrpcService::FriendAdd(grpc::ServerContext *,
                                          const zchat::FriendAddReq *request,
                                          zchat::FriendAddRsp *response) {
    ZCHAT_LOG_INFO("FriendService::FriendAdd request_id={}",
                   request->request_id());
    *response = service_->AddFriend(*request);
    LogBoundaryResponseError("FriendService", "FriendAdd",
                             request->request_id(), *response);
    return grpc::Status::OK;
}

grpc::Status
FriendGrpcService::FriendAddProcess(grpc::ServerContext *,
                                    const zchat::FriendAddProcessReq *request,
                                    zchat::FriendAddProcessRsp *response) {
    ZCHAT_LOG_INFO("FriendService::FriendAddProcess request_id={}",
                   request->request_id());
    *response = service_->ProcessFriendApply(*request);
    LogBoundaryResponseError("FriendService", "FriendAddProcess",
                             request->request_id(), *response);
    return grpc::Status::OK;
}

grpc::Status
FriendGrpcService::FriendSearch(grpc::ServerContext *,
                                const zchat::FriendSearchReq *request,
                                zchat::FriendSearchRsp *response) {
    ZCHAT_LOG_INFO("FriendService::FriendSearch request_id={}",
                   request->request_id());
    *response = service_->SearchFriend(*request);
    LogBoundaryResponseError("FriendService", "FriendSearch",
                             request->request_id(), *response);
    return grpc::Status::OK;
}

grpc::Status FriendGrpcService::GetChatSessionList(
    grpc::ServerContext *, const zchat::GetChatSessionListReq *request,
    zchat::GetChatSessionListRsp *response) {
    ZCHAT_LOG_INFO("FriendService::GetChatSessionList request_id={}",
                   request->request_id());
    *response = service_->GetChatSessionList(*request);
    LogBoundaryResponseError("FriendService", "GetChatSessionList",
                             request->request_id(), *response);
    return grpc::Status::OK;
}

grpc::Status
FriendGrpcService::ChatSessionCreate(grpc::ServerContext *,
                                     const zchat::ChatSessionCreateReq *request,
                                     zchat::ChatSessionCreateRsp *response) {
    ZCHAT_LOG_INFO("FriendService::ChatSessionCreate request_id={}",
                   request->request_id());
    *response = service_->CreateChatSession(*request);
    LogBoundaryResponseError("FriendService", "ChatSessionCreate",
                             request->request_id(), *response);
    return grpc::Status::OK;
}

grpc::Status FriendGrpcService::GetChatSessionMember(
    grpc::ServerContext *, const zchat::GetChatSessionMemberReq *request,
    zchat::GetChatSessionMemberRsp *response) {
    ZCHAT_LOG_INFO("FriendService::GetChatSessionMember request_id={}",
                   request->request_id());
    *response = service_->GetChatSessionMember(*request);
    LogBoundaryResponseError("FriendService", "GetChatSessionMember",
                             request->request_id(), *response);
    return grpc::Status::OK;
}

grpc::Status FriendGrpcService::GetPendingFriendEventList(
    grpc::ServerContext *, const zchat::GetPendingFriendEventListReq *request,
    zchat::GetPendingFriendEventListRsp *response) {
    ZCHAT_LOG_INFO("FriendService::GetPendingFriendEventList request_id={}",
                   request->request_id());
    *response = service_->GetPendingFriendEvents(*request);
    LogBoundaryResponseError("FriendService", "GetPendingFriendEventList",
                             request->request_id(), *response);
    return grpc::Status::OK;
}

} // namespace zchat
