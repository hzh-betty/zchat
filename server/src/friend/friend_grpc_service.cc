#include "friend/friend_grpc_service.h"

#include <utility>

#include "common/error_response.h"
#include "common/grpc_callback.h"
#include "common/logger.h"

namespace zchat {

FriendGrpcService::FriendGrpcService(
    std::shared_ptr<FriendApplicationService> service)
    : service_(std::move(service)) {}

#define ZCHAT_FRIEND_RPC(method_name, req_type, rsp_type, coro_name)           \
    grpc::ServerUnaryReactor *FriendGrpcService::method_name(                  \
        grpc::CallbackServerContext *, const zchat::req_type *request,         \
        zchat::rsp_type *response) {                                           \
        ZCHAT_LOG_INFO("FriendService::" #method_name " request_id={}",        \
                       request->request_id());                                 \
        return new CoroUnaryReactor<zchat::rsp_type>(                          \
            [this, req = *request]() -> drogon::Task<zchat::rsp_type> {        \
                co_return co_await service_->coro_name(req);                   \
            },                                                                 \
            response, "FriendService", #method_name, request->request_id());   \
    }

ZCHAT_FRIEND_RPC(GetFriendList, GetFriendListReq, GetFriendListRsp,
                 GetFriendListCoro)
ZCHAT_FRIEND_RPC(FriendRemove, FriendRemoveReq, FriendRemoveRsp,
                 RemoveFriendCoro)
ZCHAT_FRIEND_RPC(FriendAdd, FriendAddReq, FriendAddRsp, AddFriendCoro)
ZCHAT_FRIEND_RPC(FriendAddProcess, FriendAddProcessReq, FriendAddProcessRsp,
                 ProcessFriendApplyCoro)
ZCHAT_FRIEND_RPC(FriendSearch, FriendSearchReq, FriendSearchRsp,
                 SearchFriendCoro)
ZCHAT_FRIEND_RPC(GetChatSessionList, GetChatSessionListReq,
                 GetChatSessionListRsp, GetChatSessionListCoro)
ZCHAT_FRIEND_RPC(ChatSessionCreate, ChatSessionCreateReq, ChatSessionCreateRsp,
                 CreateChatSessionCoro)
ZCHAT_FRIEND_RPC(GetChatSessionMember, GetChatSessionMemberReq,
                 GetChatSessionMemberRsp, GetChatSessionMemberCoro)
ZCHAT_FRIEND_RPC(GetChatSessionMemberIds, GetChatSessionMemberIdsReq,
                 GetChatSessionMemberIdsRsp, GetChatSessionMemberIdsCoro)
ZCHAT_FRIEND_RPC(GetPendingFriendEventList, GetPendingFriendEventListReq,
                 GetPendingFriendEventListRsp, GetPendingFriendEventsCoro)

} // namespace zchat
