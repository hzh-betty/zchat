#ifndef ZCHAT_SERVER_SRC_USER_USER_REPOSITORY_H_
#define ZCHAT_SERVER_SRC_USER_USER_REPOSITORY_H_

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

class UserRepository : public NonCopyable {
  public:
    UserRepository() = default;

    virtual ~UserRepository() = default;

    virtual Result<std::optional<UserRecord>>
    FindUserById(const std::string &user_id) = 0;
    virtual Result<std::optional<UserRecord>>
    FindUserByNickname(const std::string &nickname) = 0;
    virtual Result<std::optional<UserRecord>>
    FindUserByPhone(const std::string &phone) = 0;
    virtual Result<std::vector<UserRecord>>
    FindUsersByIds(const std::vector<std::string> &user_ids) = 0;
    virtual Result<std::vector<UserRecord>>
    SearchUsers(const std::string &keyword,
                const std::string &exclude_user) = 0;
    virtual VoidResult InsertUser(const UserRecord &user) = 0;
    virtual VoidResult UpdateUserNickname(const std::string &user_id,
                                          const std::string &nickname) = 0;
    virtual VoidResult
    UpdateUserDescription(const std::string &user_id,
                          const std::string &description) = 0;
    virtual VoidResult UpdateUserPhone(const std::string &user_id,
                                       const std::string &phone) = 0;
    virtual VoidResult UpdateUserAvatar(const std::string &user_id,
                                        const std::string &avatar_id) = 0;
};

class OrmUserRepository final : public UserRepository,
                                public OrmRepositoryBase {
  public:
    explicit OrmUserRepository(std::shared_ptr<drogon::orm::DbClient> db);

    ~OrmUserRepository() override = default;

    Result<std::optional<UserRecord>>
    FindUserById(const std::string &user_id) override;
    Result<std::optional<UserRecord>>
    FindUserByNickname(const std::string &nickname) override;
    Result<std::optional<UserRecord>>
    FindUserByPhone(const std::string &phone) override;
    Result<std::vector<UserRecord>>
    FindUsersByIds(const std::vector<std::string> &user_ids) override;
    Result<std::vector<UserRecord>>
    SearchUsers(const std::string &keyword,
                const std::string &exclude_user) override;
    VoidResult InsertUser(const UserRecord &user) override;
    VoidResult UpdateUserNickname(const std::string &user_id,
                                  const std::string &nickname) override;
    VoidResult UpdateUserDescription(const std::string &user_id,
                                     const std::string &description) override;
    VoidResult UpdateUserPhone(const std::string &user_id,
                               const std::string &phone) override;
    VoidResult UpdateUserAvatar(const std::string &user_id,
                                const std::string &avatar_id) override;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_USER_USER_REPOSITORY_H_
