#ifndef ZCHAT_SERVER_SRC_USER_USER_REPOSITORY_H_
#define ZCHAT_SERVER_SRC_USER_USER_REPOSITORY_H_

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

class UserRepository : public NonCopyable {
  public:
    UserRepository() = default;

    virtual ~UserRepository() = default;

    virtual drogon::Task<Result<std::optional<UserRecord>>>
    FindUserByIdCoro(const std::string &user_id) = 0;
    virtual drogon::Task<Result<std::optional<UserRecord>>>
    FindUserByNicknameCoro(const std::string &nickname) = 0;
    virtual drogon::Task<Result<std::optional<UserRecord>>>
    FindUserByPhoneCoro(const std::string &phone) = 0;
    virtual drogon::Task<Result<std::vector<UserRecord>>>
    FindUsersByIdsCoro(const std::vector<std::string> &user_ids) = 0;
    virtual drogon::Task<VoidResult> InsertUserCoro(const UserRecord &user) = 0;
    virtual drogon::Task<VoidResult>
    UpdateUserNicknameCoro(const std::string &user_id,
                           const std::string &nickname) = 0;
    virtual drogon::Task<VoidResult>
    UpdateUserDescriptionCoro(const std::string &user_id,
                              const std::string &description) = 0;
    virtual drogon::Task<VoidResult>
    UpdateUserPhoneCoro(const std::string &user_id,
                        const std::string &phone) = 0;
    virtual drogon::Task<VoidResult>
    UpdateUserAvatarCoro(const std::string &user_id,
                         const std::string &avatar_id) = 0;
    virtual drogon::Task<VoidResult>
    UpdateUserPasswordCoro(const std::string &user_id,
                           const std::string &password_hash,
                           const std::string &algo) = 0;
};

class OrmUserRepository final : public UserRepository,
                                public OrmRepositoryBase {
  public:
    explicit OrmUserRepository(std::shared_ptr<drogon::orm::DbClient> db);

    ~OrmUserRepository() override = default;

    drogon::Task<Result<std::optional<UserRecord>>>
    FindUserByIdCoro(const std::string &user_id) override;
    drogon::Task<Result<std::optional<UserRecord>>>
    FindUserByNicknameCoro(const std::string &nickname) override;
    drogon::Task<Result<std::optional<UserRecord>>>
    FindUserByPhoneCoro(const std::string &phone) override;
    drogon::Task<Result<std::vector<UserRecord>>>
    FindUsersByIdsCoro(const std::vector<std::string> &user_ids) override;
    drogon::Task<VoidResult> InsertUserCoro(const UserRecord &user) override;
    drogon::Task<VoidResult>
    UpdateUserNicknameCoro(const std::string &user_id,
                           const std::string &nickname) override;
    drogon::Task<VoidResult>
    UpdateUserDescriptionCoro(const std::string &user_id,
                              const std::string &description) override;
    drogon::Task<VoidResult>
    UpdateUserPhoneCoro(const std::string &user_id,
                        const std::string &phone) override;
    drogon::Task<VoidResult>
    UpdateUserAvatarCoro(const std::string &user_id,
                         const std::string &avatar_id) override;
    drogon::Task<VoidResult>
    UpdateUserPasswordCoro(const std::string &user_id,
                           const std::string &password_hash,
                           const std::string &algo) override;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_USER_USER_REPOSITORY_H_
