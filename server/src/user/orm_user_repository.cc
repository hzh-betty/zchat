#include "user/user_repository.h"

#include <utility>

#include <drogon/orm/DbClient.h>
#include <drogon/orm/SqlBinder.h>

namespace zchat {

OrmUserRepository::OrmUserRepository(std::shared_ptr<drogon::orm::DbClient> db)
    : OrmRepositoryBase(std::move(db)) {}

drogon::Task<Result<std::optional<UserRecord>>>
OrmUserRepository::FindUserByIdCoro(const std::string &user_id) {
    return RunDbCoro([&]() -> drogon::Task<Result<std::optional<UserRecord>>> {
        const auto result = co_await db_->execSqlCoro(
            "SELECT user_id,nickname,description,password,"
            "phone,avatar_id FROM `user` WHERE user_id=? LIMIT 1",
            user_id);
        if (result.empty()) {
            co_return Result<std::optional<UserRecord>>::Ok(std::nullopt);
        }
        co_return Result<std::optional<UserRecord>>::Ok(
            ToUserRecord(result[0]));
    });
}

drogon::Task<Result<std::optional<UserRecord>>>
OrmUserRepository::FindUserByNicknameCoro(const std::string &nickname) {
    return RunDbCoro([&]() -> drogon::Task<Result<std::optional<UserRecord>>> {
        const auto result = co_await db_->execSqlCoro(
            "SELECT user_id,nickname,description,password,"
            "phone,avatar_id FROM `user` WHERE nickname=? LIMIT 1",
            nickname);
        if (result.empty()) {
            co_return Result<std::optional<UserRecord>>::Ok(std::nullopt);
        }
        co_return Result<std::optional<UserRecord>>::Ok(
            ToUserRecord(result[0]));
    });
}

drogon::Task<Result<std::optional<UserRecord>>>
OrmUserRepository::FindUserByPhoneCoro(const std::string &phone) {
    return RunDbCoro([&]() -> drogon::Task<Result<std::optional<UserRecord>>> {
        const auto result = co_await db_->execSqlCoro(
            "SELECT user_id,nickname,description,password,"
            "phone,avatar_id FROM `user` WHERE phone=? LIMIT 1",
            phone);
        if (result.empty()) {
            co_return Result<std::optional<UserRecord>>::Ok(std::nullopt);
        }
        co_return Result<std::optional<UserRecord>>::Ok(
            ToUserRecord(result[0]));
    });
}

drogon::Task<Result<std::vector<UserRecord>>>
OrmUserRepository::FindUsersByIdsCoro(
    const std::vector<std::string> &user_ids) {
    if (user_ids.empty()) {
        co_return Result<std::vector<UserRecord>>::Ok({});
    }
    co_return co_await RunDbCoro(
        [&]() -> drogon::Task<Result<std::vector<UserRecord>>> {
            std::string sql = "SELECT user_id,nickname,description,password,"
                              "phone,avatar_id FROM `user` "
                              "WHERE user_id IN ('";
            for (std::size_t i = 0; i < user_ids.size(); ++i) {
                if (i > 0) {
                    sql += "','";
                }
                sql += user_ids[i];
            }
            sql += "')";
            const auto result = co_await db_->execSqlCoro(sql);
            std::vector<UserRecord> users;
            for (const auto &row : result) {
                users.push_back(ToUserRecord(row));
            }
            co_return Result<std::vector<UserRecord>>::Ok(std::move(users));
        });
}

drogon::Task<VoidResult>
OrmUserRepository::InsertUserCoro(const UserRecord &user) {
    return RunDbCoro([&]() -> drogon::Task<VoidResult> {
        co_await db_->execSqlCoro(
            "INSERT INTO `user` "
            "(user_id,nickname,description,password,phone,avatar_id) "
            "VALUES (?,?,NULLIF(?, ''),?,NULLIF(?, ''),NULLIF(?, ''))",
            user.user_id, user.nickname.empty() ? user.user_id : user.nickname,
            user.description, user.password, user.phone, user.avatar_id);
        co_return VoidResult::Ok();
    });
}

drogon::Task<VoidResult>
OrmUserRepository::UpdateUserNicknameCoro(const std::string &user_id,
                                          const std::string &nickname) {
    return RunDbCoro([&]() -> drogon::Task<VoidResult> {
        co_await db_->execSqlCoro(
            "UPDATE `user` SET nickname=? WHERE user_id=?", nickname, user_id);
        co_return VoidResult::Ok();
    });
}

drogon::Task<VoidResult>
OrmUserRepository::UpdateUserDescriptionCoro(const std::string &user_id,
                                             const std::string &description) {
    return RunDbCoro([&]() -> drogon::Task<VoidResult> {
        co_await db_->execSqlCoro(
            "UPDATE `user` SET description=? WHERE user_id=?", description,
            user_id);
        co_return VoidResult::Ok();
    });
}

drogon::Task<VoidResult>
OrmUserRepository::UpdateUserPhoneCoro(const std::string &user_id,
                                       const std::string &phone) {
    return RunDbCoro([&]() -> drogon::Task<VoidResult> {
        co_await db_->execSqlCoro("UPDATE `user` SET phone=? WHERE user_id=?",
                                  phone, user_id);
        co_return VoidResult::Ok();
    });
}

drogon::Task<VoidResult>
OrmUserRepository::UpdateUserAvatarCoro(const std::string &user_id,
                                        const std::string &avatar_id) {
    return RunDbCoro([&]() -> drogon::Task<VoidResult> {
        co_await db_->execSqlCoro(
            "UPDATE `user` SET avatar_id=? WHERE user_id=?", avatar_id,
            user_id);
        co_return VoidResult::Ok();
    });
}

} // namespace zchat
