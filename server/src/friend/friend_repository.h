#ifndef ZCHAT_SERVER_SRC_FRIEND_FRIEND_REPOSITORY_H_
#define ZCHAT_SERVER_SRC_FRIEND_FRIEND_REPOSITORY_H_

#include "common/noncopyable.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <drogon/orm/DbClient.h>

#include "common/domain_records.h"
#include "common/orm_helpers.h"
#include "common/result.h"

namespace zchat {

class FriendRepository : public NonCopyable {
  public:
    FriendRepository() = default;

    virtual ~FriendRepository() = default;

    virtual VoidResult InsertRelation(const std::string &user_id,
                                      const std::string &peer_id) = 0;
    virtual VoidResult DeleteRelation(const std::string &user_id,
                                      const std::string &peer_id) = 0;
    virtual Result<bool> RelationExists(const std::string &user_id,
                                        const std::string &peer_id) = 0;
    virtual Result<std::vector<std::string>>
    ListFriendIds(const std::string &user_id) = 0;
    virtual VoidResult InsertFriendApply(const FriendApplyRecord &apply) = 0;
    virtual VoidResult DeleteFriendApply(const std::string &user_id,
                                         const std::string &peer_id) = 0;
    virtual Result<bool> FriendApplyExists(const std::string &user_id,
                                           const std::string &peer_id) = 0;
    virtual Result<std::vector<FriendApplyRecord>>
    ListPendingApplies(const std::string &peer_id) = 0;
    virtual VoidResult InsertChatSession(const ChatSessionRecord &session) = 0;
    virtual VoidResult InsertChatSessionMember(const std::string &session_id,
                                               const std::string &user_id) = 0;
    virtual VoidResult InsertChatSessionMembers(
        const std::string &session_id,
        const std::vector<std::string> &user_ids) = 0;
    virtual VoidResult DeleteSingleChatSession(const std::string &user_id,
                                               const std::string &peer_id) = 0;
    virtual Result<std::vector<ChatSessionRecord>>
    ListChatSessions(const std::string &user_id) = 0;
    virtual Result<std::vector<std::string>>
    ListChatSessionMembers(const std::string &session_id) = 0;
    virtual Result<std::optional<std::string>>
    FindSingleChatPeer(const std::string &session_id,
                       const std::string &user_id) = 0;
};

class OrmFriendRepository final : public FriendRepository,
                                  public OrmRepositoryBase {
  public:
    explicit OrmFriendRepository(std::shared_ptr<drogon::orm::DbClient> db);

    ~OrmFriendRepository() override = default;

    VoidResult InsertRelation(const std::string &user_id,
                              const std::string &peer_id) override;
    VoidResult DeleteRelation(const std::string &user_id,
                              const std::string &peer_id) override;
    Result<bool> RelationExists(const std::string &user_id,
                                const std::string &peer_id) override;
    Result<std::vector<std::string>>
    ListFriendIds(const std::string &user_id) override;
    VoidResult InsertFriendApply(const FriendApplyRecord &apply) override;
    VoidResult DeleteFriendApply(const std::string &user_id,
                                 const std::string &peer_id) override;
    Result<bool> FriendApplyExists(const std::string &user_id,
                                   const std::string &peer_id) override;
    Result<std::vector<FriendApplyRecord>>
    ListPendingApplies(const std::string &peer_id) override;
    VoidResult InsertChatSession(const ChatSessionRecord &session) override;
    VoidResult InsertChatSessionMember(const std::string &session_id,
                                       const std::string &user_id) override;
    VoidResult InsertChatSessionMembers(
        const std::string &session_id,
        const std::vector<std::string> &user_ids) override;
    VoidResult DeleteSingleChatSession(const std::string &user_id,
                                       const std::string &peer_id) override;
    Result<std::vector<ChatSessionRecord>>
    ListChatSessions(const std::string &user_id) override;
    Result<std::vector<std::string>>
    ListChatSessionMembers(const std::string &session_id) override;
    Result<std::optional<std::string>>
    FindSingleChatPeer(const std::string &session_id,
                       const std::string &user_id) override;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_FRIEND_FRIEND_REPOSITORY_H_
