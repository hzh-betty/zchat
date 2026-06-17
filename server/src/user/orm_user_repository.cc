#include "user/user_repository.h"

#include <utility>

#include <drogon/orm/DbClient.h>
#include <drogon/orm/SqlBinder.h>

namespace zchat {

OrmUserRepository::OrmUserRepository(std::shared_ptr<drogon::orm::DbClient> db)
    : OrmRepositoryBase(std::move(db)) {}

Result<std::optional<UserRecord>>
OrmUserRepository::FindUserById(const std::string &user_id) {
    return RunDb([&]() {
        const auto result = db_->execSqlSync(
            "SELECT user_id,nickname,description,password,phone,avatar_id "
            "FROM `user` WHERE user_id=? LIMIT 1",
            user_id);
        if (result.empty()) {
            return Result<std::optional<UserRecord>>::Ok(std::nullopt);
        }
        return Result<std::optional<UserRecord>>::Ok(ToUserRecord(result[0]));
    });
}

Result<std::optional<UserRecord>>
OrmUserRepository::FindUserByNickname(const std::string &nickname) {
    return RunDb([&]() {
        const auto result = db_->execSqlSync(
            "SELECT user_id,nickname,description,password,phone,avatar_id "
            "FROM `user` WHERE nickname=? LIMIT 1",
            nickname);
        if (result.empty()) {
            return Result<std::optional<UserRecord>>::Ok(std::nullopt);
        }
        return Result<std::optional<UserRecord>>::Ok(ToUserRecord(result[0]));
    });
}

Result<std::optional<UserRecord>>
OrmUserRepository::FindUserByPhone(const std::string &phone) {
    return RunDb([&]() {
        const auto result = db_->execSqlSync(
            "SELECT user_id,nickname,description,password,phone,avatar_id "
            "FROM `user` WHERE phone=? LIMIT 1",
            phone);
        if (result.empty()) {
            return Result<std::optional<UserRecord>>::Ok(std::nullopt);
        }
        return Result<std::optional<UserRecord>>::Ok(ToUserRecord(result[0]));
    });
}

Result<std::vector<UserRecord>>
OrmUserRepository::FindUsersByIds(const std::vector<std::string> &user_ids) {
    if (user_ids.empty()) {
        return Result<std::vector<UserRecord>>::Ok({});
    }
    return RunDb([&]() -> Result<std::vector<UserRecord>> {
        std::string placeholders;
        for (std::size_t i = 0; i < user_ids.size(); ++i) {
            if (i > 0) {
                placeholders += ",";
            }
            placeholders += "?";
        }
        auto binder = (*db_ << ("SELECT user_id,nickname,description,password,"
                                "phone,avatar_id FROM `user` "
                                "WHERE user_id IN (" +
                                placeholders + ")"));
        for (const auto &id : user_ids) {
            binder << id;
        }
        binder << drogon::orm::Mode::Blocking;
        drogon::orm::Result result(nullptr);
        binder >> [&result](const drogon::orm::Result &r) { result = r; };
        binder.exec();
        std::vector<UserRecord> users;
        for (const auto &row : result) {
            users.push_back(ToUserRecord(row));
        }
        return Result<std::vector<UserRecord>>::Ok(std::move(users));
    });
}

VoidResult OrmUserRepository::InsertUser(const UserRecord &user) {
    return RunDb([&]() {
        db_->execSqlSync(
            "INSERT INTO `user` "
            "(user_id,nickname,description,password,phone,avatar_id) "
            "VALUES (?,?,NULLIF(?, ''),?,NULLIF(?, ''),NULLIF(?, ''))",
            user.user_id, user.nickname.empty() ? user.user_id : user.nickname,
            user.description, user.password, user.phone, user.avatar_id);
        return VoidResult::Ok();
    });
}

VoidResult OrmUserRepository::UpdateUserNickname(const std::string &user_id,
                                                 const std::string &nickname) {
    return RunDb([&]() {
        db_->execSqlSync("UPDATE `user` SET nickname=? WHERE user_id=?",
                         nickname, user_id);
        return VoidResult::Ok();
    });
}

VoidResult
OrmUserRepository::UpdateUserDescription(const std::string &user_id,
                                         const std::string &description) {
    return RunDb([&]() {
        db_->execSqlSync("UPDATE `user` SET description=? WHERE user_id=?",
                         description, user_id);
        return VoidResult::Ok();
    });
}

VoidResult OrmUserRepository::UpdateUserPhone(const std::string &user_id,
                                              const std::string &phone) {
    return RunDb([&]() {
        db_->execSqlSync("UPDATE `user` SET phone=? WHERE user_id=?", phone,
                         user_id);
        return VoidResult::Ok();
    });
}

VoidResult OrmUserRepository::UpdateUserAvatar(const std::string &user_id,
                                               const std::string &avatar_id) {
    return RunDb([&]() {
        db_->execSqlSync("UPDATE `user` SET avatar_id=? WHERE user_id=?",
                         avatar_id, user_id);
        return VoidResult::Ok();
    });
}

} // namespace zchat
