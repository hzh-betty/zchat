#ifndef ZCHAT_SERVER_SRC_FRIEND_FRIEND_REPOSITORY_H_
#define ZCHAT_SERVER_SRC_FRIEND_FRIEND_REPOSITORY_H_

#include "common/noncopyable.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <drogon/orm/DbClient.h>
#include <drogon/utils/coroutine.h>

#include "common/domain_records.h"
#include "common/orm_helpers.h"
#include "common/result.h"

namespace zchat {

class FriendRepository : public NonCopyable {
  public:
    FriendRepository() = default;
    virtual ~FriendRepository() = default;

    virtual drogon::Task<VoidResult>
    InsertRelationCoro(const std::string &user_id,
                       const std::string &peer_id) = 0;
    virtual drogon::Task<VoidResult>
    DeleteRelationCoro(const std::string &user_id,
                       const std::string &peer_id) = 0;
    virtual drogon::Task<Result<bool>>
    RelationExistsCoro(const std::string &user_id,
                       const std::string &peer_id) = 0;
    virtual drogon::Task<Result<std::vector<std::string>>>
    ListExistingPeersCoro(const std::string &user_id,
                          const std::vector<std::string> &peer_ids) = 0;
    virtual drogon::Task<Result<std::vector<std::string>>>
    ListFriendIdsCoro(const std::string &user_id) = 0;
    virtual drogon::Task<VoidResult>
    InsertFriendApplyCoro(const FriendApplyRecord &apply) = 0;
    virtual drogon::Task<VoidResult>
    DeleteFriendApplyCoro(const std::string &user_id,
                          const std::string &peer_id) = 0;
    virtual drogon::Task<Result<bool>>
    FriendApplyExistsCoro(const std::string &user_id,
                          const std::string &peer_id) = 0;
    virtual drogon::Task<Result<std::vector<FriendApplyRecord>>>
    ListPendingAppliesCoro(const std::string &peer_id) = 0;
    virtual drogon::Task<VoidResult>
    InsertChatSessionCoro(const ChatSessionRecord &session) = 0;
    virtual drogon::Task<VoidResult>
    InsertChatSessionMemberCoro(const std::string &session_id,
                                const std::string &user_id) = 0;
    virtual drogon::Task<VoidResult>
    InsertChatSessionMembersCoro(const std::string &session_id,
                                 const std::vector<std::string> &user_ids) = 0;
    virtual drogon::Task<VoidResult>
    DeleteSingleChatSessionCoro(const std::string &user_id,
                                const std::string &peer_id) = 0;
    virtual drogon::Task<Result<std::vector<ChatSessionRecord>>>
    ListChatSessionsCoro(const std::string &user_id) = 0;
    virtual drogon::Task<Result<std::vector<std::string>>>
    ListChatSessionMembersCoro(const std::string &session_id) = 0;
    virtual drogon::Task<Result<std::optional<std::string>>>
    FindSingleChatPeerCoro(const std::string &session_id,
                           const std::string &user_id) = 0;
};

class OrmFriendRepository final : public FriendRepository,
                                  public OrmRepositoryBase {
  public:
    explicit OrmFriendRepository(std::shared_ptr<drogon::orm::DbClient> db);
    ~OrmFriendRepository() override = default;

    drogon::Task<VoidResult>
    InsertRelationCoro(const std::string &user_id,
                       const std::string &peer_id) override;
    drogon::Task<VoidResult>
    DeleteRelationCoro(const std::string &user_id,
                       const std::string &peer_id) override;
    drogon::Task<Result<bool>>
    RelationExistsCoro(const std::string &user_id,
                       const std::string &peer_id) override;
    drogon::Task<Result<std::vector<std::string>>>
    ListExistingPeersCoro(const std::string &user_id,
                          const std::vector<std::string> &peer_ids) override;
    drogon::Task<Result<std::vector<std::string>>>
    ListFriendIdsCoro(const std::string &user_id) override;
    drogon::Task<VoidResult>
    InsertFriendApplyCoro(const FriendApplyRecord &apply) override;
    drogon::Task<VoidResult>
    DeleteFriendApplyCoro(const std::string &user_id,
                          const std::string &peer_id) override;
    drogon::Task<Result<bool>>
    FriendApplyExistsCoro(const std::string &user_id,
                          const std::string &peer_id) override;
    drogon::Task<Result<std::vector<FriendApplyRecord>>>
    ListPendingAppliesCoro(const std::string &peer_id) override;
    drogon::Task<VoidResult>
    InsertChatSessionCoro(const ChatSessionRecord &session) override;
    drogon::Task<VoidResult>
    InsertChatSessionMemberCoro(const std::string &session_id,
                                const std::string &user_id) override;
    drogon::Task<VoidResult> InsertChatSessionMembersCoro(
        const std::string &session_id,
        const std::vector<std::string> &user_ids) override;
    drogon::Task<VoidResult>
    DeleteSingleChatSessionCoro(const std::string &user_id,
                                const std::string &peer_id) override;
    drogon::Task<Result<std::vector<ChatSessionRecord>>>
    ListChatSessionsCoro(const std::string &user_id) override;
    drogon::Task<Result<std::vector<std::string>>>
    ListChatSessionMembersCoro(const std::string &session_id) override;
    drogon::Task<Result<std::optional<std::string>>>
    FindSingleChatPeerCoro(const std::string &session_id,
                           const std::string &user_id) override;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_FRIEND_FRIEND_REPOSITORY_H_
