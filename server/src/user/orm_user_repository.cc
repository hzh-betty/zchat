#include "user/user_repository.h"

#include <utility>

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
    std::vector<UserRecord> users;
    for (const auto &user_id : user_ids) {
        auto user = FindUserById(user_id);
        if (!user.ok()) {
            return Result<std::vector<UserRecord>>::Fail(user.error().message);
        }
        if (user.value().has_value()) {
            users.push_back(user.value().value());
        }
    }
    return Result<std::vector<UserRecord>>::Ok(std::move(users));
}

Result<std::vector<UserRecord>>
OrmUserRepository::SearchUsers(const std::string &keyword,
                               const std::string &exclude_user) {
    return RunDb([&]() {
        const std::string like = "%" + keyword + "%";
        const auto result = db_->execSqlSync(
            "SELECT user_id,nickname,description,password,phone,avatar_id "
            "FROM `user` WHERE user_id<>? AND "
            "(user_id LIKE ? OR nickname LIKE ? OR phone LIKE ?) LIMIT 50",
            exclude_user, like, like, like);
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
