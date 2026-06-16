#ifndef ZCHAT_SERVER_SRC_FRIEND_FRIEND_GRPC_SERVICE_H_
#define ZCHAT_SERVER_SRC_FRIEND_FRIEND_GRPC_SERVICE_H_

#include "common/noncopyable.h"

#include <memory>

#include <grpcpp/grpcpp.h>

#include "friend.grpc.pb.h"
#include "friend/friend_service.h"

namespace zchat {

class FriendGrpcService final : public zchat::FriendService::Service,
                                public NonCopyable {
  public:
    explicit FriendGrpcService(
        std::shared_ptr<FriendApplicationService> service);

    ~FriendGrpcService() override = default;

    grpc::Status GetFriendList(grpc::ServerContext *context,
                               const zchat::GetFriendListReq *request,
                               zchat::GetFriendListRsp *response) override;
    grpc::Status FriendRemove(grpc::ServerContext *context,
                              const zchat::FriendRemoveReq *request,
                              zchat::FriendRemoveRsp *response) override;
    grpc::Status FriendAdd(grpc::ServerContext *context,
                           const zchat::FriendAddReq *request,
                           zchat::FriendAddRsp *response) override;
    grpc::Status
    FriendAddProcess(grpc::ServerContext *context,
                     const zchat::FriendAddProcessReq *request,
                     zchat::FriendAddProcessRsp *response) override;
    grpc::Status FriendSearch(grpc::ServerContext *context,
                              const zchat::FriendSearchReq *request,
                              zchat::FriendSearchRsp *response) override;
    grpc::Status
    GetChatSessionList(grpc::ServerContext *context,
                       const zchat::GetChatSessionListReq *request,
                       zchat::GetChatSessionListRsp *response) override;
    grpc::Status
    ChatSessionCreate(grpc::ServerContext *context,
                      const zchat::ChatSessionCreateReq *request,
                      zchat::ChatSessionCreateRsp *response) override;
    grpc::Status
    GetChatSessionMember(grpc::ServerContext *context,
                         const zchat::GetChatSessionMemberReq *request,
                         zchat::GetChatSessionMemberRsp *response) override;
    grpc::Status GetChatSessionMemberIds(
        grpc::ServerContext *context,
        const zchat::GetChatSessionMemberIdsReq *request,
        zchat::GetChatSessionMemberIdsRsp *response) override;
    grpc::Status GetPendingFriendEventList(
        grpc::ServerContext *context,
        const zchat::GetPendingFriendEventListReq *request,
        zchat::GetPendingFriendEventListRsp *response) override;

  private:
    std::shared_ptr<FriendApplicationService> service_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_FRIEND_FRIEND_GRPC_SERVICE_H_
