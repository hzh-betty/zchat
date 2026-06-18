#ifndef ZCHAT_SERVER_SRC_FRIEND_FRIEND_GRPC_SERVICE_H_
#define ZCHAT_SERVER_SRC_FRIEND_FRIEND_GRPC_SERVICE_H_

#include "common/noncopyable.h"

#include <memory>

#include <grpcpp/grpcpp.h>

#include "friend.grpc.pb.h"
#include "friend/friend_service.h"

namespace zchat {

class FriendGrpcService final : public zchat::FriendService::CallbackService,
                                public NonCopyable {
  public:
    explicit FriendGrpcService(
        std::shared_ptr<FriendApplicationService> service);
    ~FriendGrpcService() override = default;

    grpc::ServerUnaryReactor *
    GetFriendList(grpc::CallbackServerContext *context,
                  const zchat::GetFriendListReq *request,
                  zchat::GetFriendListRsp *response) override;
    grpc::ServerUnaryReactor *
    FriendRemove(grpc::CallbackServerContext *context,
                 const zchat::FriendRemoveReq *request,
                 zchat::FriendRemoveRsp *response) override;
    grpc::ServerUnaryReactor *FriendAdd(grpc::CallbackServerContext *context,
                                        const zchat::FriendAddReq *request,
                                        zchat::FriendAddRsp *response) override;
    grpc::ServerUnaryReactor *
    FriendAddProcess(grpc::CallbackServerContext *context,
                     const zchat::FriendAddProcessReq *request,
                     zchat::FriendAddProcessRsp *response) override;
    grpc::ServerUnaryReactor *
    FriendSearch(grpc::CallbackServerContext *context,
                 const zchat::FriendSearchReq *request,
                 zchat::FriendSearchRsp *response) override;
    grpc::ServerUnaryReactor *
    GetChatSessionList(grpc::CallbackServerContext *context,
                       const zchat::GetChatSessionListReq *request,
                       zchat::GetChatSessionListRsp *response) override;
    grpc::ServerUnaryReactor *
    ChatSessionCreate(grpc::CallbackServerContext *context,
                      const zchat::ChatSessionCreateReq *request,
                      zchat::ChatSessionCreateRsp *response) override;
    grpc::ServerUnaryReactor *
    GetChatSessionMember(grpc::CallbackServerContext *context,
                         const zchat::GetChatSessionMemberReq *request,
                         zchat::GetChatSessionMemberRsp *response) override;
    grpc::ServerUnaryReactor *GetChatSessionMemberIds(
        grpc::CallbackServerContext *context,
        const zchat::GetChatSessionMemberIdsReq *request,
        zchat::GetChatSessionMemberIdsRsp *response) override;
    grpc::ServerUnaryReactor *GetPendingFriendEventList(
        grpc::CallbackServerContext *context,
        const zchat::GetPendingFriendEventListReq *request,
        zchat::GetPendingFriendEventListRsp *response) override;

  private:
    std::shared_ptr<FriendApplicationService> service_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_FRIEND_FRIEND_GRPC_SERVICE_H_
