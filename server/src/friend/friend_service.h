#ifndef ZCHAT_SERVER_SRC_FRIEND_FRIEND_SERVICE_H_
#define ZCHAT_SERVER_SRC_FRIEND_FRIEND_SERVICE_H_

#include "common/noncopyable.h"

#include <string>
#include <unordered_map>

#include <drogon/utils/coroutine.h>

#include "common/notify_publisher.h"
#include "common/service_clients.h"
#include "common/session_store.h"
#include "friend.pb.h"
#include "friend/friend_repository.h"
#include "notify.pb.h"

namespace zchat {

class FriendApplicationService : public NonCopyable {
  public:
    FriendApplicationService(FriendRepository &friends, ServiceClients &clients,
                             SessionStore &sessions, NotifyPublisher &notifier);
    ~FriendApplicationService() = default;

    drogon::Task<zchat::GetFriendListRsp>
    GetFriendListCoro(const zchat::GetFriendListReq &request);
    drogon::Task<zchat::GetChatSessionListRsp>
    GetChatSessionListCoro(const zchat::GetChatSessionListReq &request);
    drogon::Task<zchat::GetPendingFriendEventListRsp>
    GetPendingFriendEventsCoro(
        const zchat::GetPendingFriendEventListReq &request);
    drogon::Task<zchat::FriendRemoveRsp>
    RemoveFriendCoro(const zchat::FriendRemoveReq &request);
    drogon::Task<zchat::FriendAddRsp>
    AddFriendCoro(const zchat::FriendAddReq &request);
    drogon::Task<zchat::FriendAddProcessRsp>
    ProcessFriendApplyCoro(const zchat::FriendAddProcessReq &request);
    drogon::Task<zchat::ChatSessionCreateRsp>
    CreateChatSessionCoro(const zchat::ChatSessionCreateReq &request);
    drogon::Task<zchat::GetChatSessionMemberRsp>
    GetChatSessionMemberCoro(const zchat::GetChatSessionMemberReq &request);
    drogon::Task<zchat::GetChatSessionMemberIdsRsp> GetChatSessionMemberIdsCoro(
        const zchat::GetChatSessionMemberIdsReq &request);
    drogon::Task<zchat::FriendSearchRsp>
    SearchFriendCoro(const zchat::FriendSearchReq &request);

  private:
    drogon::Task<std::string>
    ResolveUserIdCoro(const std::string &session_id,
                      const std::string &optional_user_id);
    drogon::Task<zchat::UserInfo> UserInfoForIdCoro(const std::string &user_id);
    drogon::Task<zchat::ChatSessionInfo> BuildChatSessionInfoCoro(
        const ChatSessionRecord &session, const std::string &current_user_id,
        const std::unordered_map<std::string, zchat::UserInfo> &peer_infos,
        const std::unordered_map<std::string, zchat::MessageInfo> &recent_msgs);
    drogon::Task<VoidResult> NotifyUserCoro(const std::string &user_id,
                                            const zchat::NotifyMessage &msg);
    drogon::Task<VoidResult>
    NotifyUsersCoro(const std::vector<std::string> &user_ids,
                    const zchat::NotifyMessage &msg);

    FriendRepository &friends_;
    ServiceClients &clients_;
    SessionStore &sessions_;
    NotifyPublisher &notifier_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_FRIEND_FRIEND_SERVICE_H_
