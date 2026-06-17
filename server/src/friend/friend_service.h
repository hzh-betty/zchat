#ifndef ZCHAT_SERVER_SRC_FRIEND_FRIEND_SERVICE_H_
#define ZCHAT_SERVER_SRC_FRIEND_FRIEND_SERVICE_H_

#include "common/noncopyable.h"

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

    zchat::GetFriendListRsp
    GetFriendList(const zchat::GetFriendListReq &request);
    zchat::GetChatSessionListRsp
    GetChatSessionList(const zchat::GetChatSessionListReq &request);
    zchat::GetPendingFriendEventListRsp
    GetPendingFriendEvents(const zchat::GetPendingFriendEventListReq &request);
    zchat::FriendRemoveRsp RemoveFriend(const zchat::FriendRemoveReq &request);
    zchat::FriendAddRsp AddFriend(const zchat::FriendAddReq &request);
    zchat::FriendAddProcessRsp
    ProcessFriendApply(const zchat::FriendAddProcessReq &request);
    zchat::ChatSessionCreateRsp
    CreateChatSession(const zchat::ChatSessionCreateReq &request);
    zchat::GetChatSessionMemberRsp
    GetChatSessionMember(const zchat::GetChatSessionMemberReq &request);
    zchat::GetChatSessionMemberIdsRsp
    GetChatSessionMemberIds(const zchat::GetChatSessionMemberIdsReq &request);
    zchat::FriendSearchRsp SearchFriend(const zchat::FriendSearchReq &request);

  private:
    std::string ResolveUserId(const std::string &session_id,
                              const std::string &optional_user_id);
    std::string AvatarForUserId(const std::string &avatar_id);
    zchat::UserInfo UserInfoForId(const std::string &user_id);
    zchat::ChatSessionInfo
    BuildChatSessionInfo(const ChatSessionRecord &session,
                         const std::string &current_user_id);
    void NotifyUser(const std::string &user_id,
                    const zchat::NotifyMessage &msg);
    void NotifyUsers(const std::vector<std::string> &user_ids,
                     const zchat::NotifyMessage &msg);

    FriendRepository &friends_;
    ServiceClients &clients_;
    SessionStore &sessions_;
    NotifyPublisher &notifier_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_FRIEND_FRIEND_SERVICE_H_