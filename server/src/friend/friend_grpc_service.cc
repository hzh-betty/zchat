#include "friend/friend_grpc_service.h"

#include <utility>

namespace zchat {

FriendGrpcService::FriendGrpcService(
    std::shared_ptr<FriendApplicationService> service)
    : service_(std::move(service)) {}

grpc::Status
FriendGrpcService::GetFriendList(grpc::ServerContext *,
                                 const zchat::GetFriendListReq *request,
                                 zchat::GetFriendListRsp *response) {
    *response = service_->GetFriendList(*request);
    return grpc::Status::OK;
}

grpc::Status
FriendGrpcService::FriendRemove(grpc::ServerContext *,
                                const zchat::FriendRemoveReq *request,
                                zchat::FriendRemoveRsp *response) {
    *response = service_->RemoveFriend(*request);
    return grpc::Status::OK;
}

grpc::Status FriendGrpcService::FriendAdd(grpc::ServerContext *,
                                          const zchat::FriendAddReq *request,
                                          zchat::FriendAddRsp *response) {
    *response = service_->AddFriend(*request);
    return grpc::Status::OK;
}

grpc::Status
FriendGrpcService::FriendAddProcess(grpc::ServerContext *,
                                    const zchat::FriendAddProcessReq *request,
                                    zchat::FriendAddProcessRsp *response) {
    *response = service_->ProcessFriendApply(*request);
    return grpc::Status::OK;
}

grpc::Status
FriendGrpcService::FriendSearch(grpc::ServerContext *,
                                const zchat::FriendSearchReq *request,
                                zchat::FriendSearchRsp *response) {
    *response = service_->SearchFriend(*request);
    return grpc::Status::OK;
}

grpc::Status FriendGrpcService::GetChatSessionList(
    grpc::ServerContext *, const zchat::GetChatSessionListReq *request,
    zchat::GetChatSessionListRsp *response) {
    *response = service_->GetChatSessionList(*request);
    return grpc::Status::OK;
}

grpc::Status
FriendGrpcService::ChatSessionCreate(grpc::ServerContext *,
                                     const zchat::ChatSessionCreateReq *request,
                                     zchat::ChatSessionCreateRsp *response) {
    *response = service_->CreateChatSession(*request);
    return grpc::Status::OK;
}

grpc::Status FriendGrpcService::GetChatSessionMember(
    grpc::ServerContext *, const zchat::GetChatSessionMemberReq *request,
    zchat::GetChatSessionMemberRsp *response) {
    *response = service_->GetChatSessionMember(*request);
    return grpc::Status::OK;
}

grpc::Status FriendGrpcService::GetPendingFriendEventList(
    grpc::ServerContext *, const zchat::GetPendingFriendEventListReq *request,
    zchat::GetPendingFriendEventListRsp *response) {
    *response = service_->GetPendingFriendEvents(*request);
    return grpc::Status::OK;
}

} // namespace zchat
