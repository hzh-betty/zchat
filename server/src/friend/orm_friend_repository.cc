#include "friend/friend_repository.h"

#include <utility>

#include <drogon/orm/DbClient.h>
#include <drogon/orm/SqlBinder.h>

namespace zchat {

OrmFriendRepository::OrmFriendRepository(
    std::shared_ptr<drogon::orm::DbClient> db)
    : OrmRepositoryBase(std::move(db)) {}

VoidResult OrmFriendRepository::InsertRelation(const std::string &user_id,
                                               const std::string &peer_id) {
    return RunDb([&]() {
        db_->execSqlSync(
            "INSERT IGNORE INTO `relation` (user_id,peer_id) VALUES (?,?)",
            user_id, peer_id);
        db_->execSqlSync(
            "INSERT IGNORE INTO `relation` (user_id,peer_id) VALUES (?,?)",
            peer_id, user_id);
        return VoidResult::Ok();
    });
}

VoidResult OrmFriendRepository::DeleteRelation(const std::string &user_id,
                                               const std::string &peer_id) {
    return RunDb([&]() {
        db_->execSqlSync(
            "DELETE FROM `relation` WHERE (user_id=? AND peer_id=?) OR "
            "(user_id=? AND peer_id=?)",
            user_id, peer_id, peer_id, user_id);
        return VoidResult::Ok();
    });
}

Result<bool> OrmFriendRepository::RelationExists(const std::string &user_id,
                                                 const std::string &peer_id) {
    return RunDb([&]() {
        const auto result = db_->execSqlSync(
            "SELECT id FROM `relation` WHERE user_id=? AND peer_id=? LIMIT 1",
            user_id, peer_id);
        return Result<bool>::Ok(!result.empty());
    });
}

Result<std::vector<std::string>>
OrmFriendRepository::ListFriendIds(const std::string &user_id) {
    return RunDb([&]() {
        const auto result = db_->execSqlSync(
            "SELECT peer_id FROM `relation` WHERE user_id=? ORDER BY id DESC",
            user_id);
        std::vector<std::string> ids;
        for (const auto &row : result) {
            ids.push_back(FieldString(row, "peer_id"));
        }
        return Result<std::vector<std::string>>::Ok(std::move(ids));
    });
}

VoidResult
OrmFriendRepository::InsertFriendApply(const FriendApplyRecord &apply) {
    return RunDb([&]() {
        db_->execSqlSync("INSERT INTO `friend_apply` "
                         "(event_id,user_id,peer_id) VALUES (?,?,?)",
                         apply.event_id, apply.user_id, apply.peer_id);
        return VoidResult::Ok();
    });
}

VoidResult OrmFriendRepository::DeleteFriendApply(const std::string &user_id,
                                                  const std::string &peer_id) {
    return RunDb([&]() {
        db_->execSqlSync(
            "DELETE FROM `friend_apply` WHERE user_id=? AND peer_id=?", user_id,
            peer_id);
        return VoidResult::Ok();
    });
}

Result<bool>
OrmFriendRepository::FriendApplyExists(const std::string &user_id,
                                       const std::string &peer_id) {
    return RunDb([&]() {
        const auto result =
            db_->execSqlSync("SELECT id FROM `friend_apply` WHERE user_id=? "
                             "AND peer_id=? LIMIT 1",
                             user_id, peer_id);
        return Result<bool>::Ok(!result.empty());
    });
}

Result<std::vector<FriendApplyRecord>>
OrmFriendRepository::ListPendingApplies(const std::string &peer_id) {
    return RunDb([&]() {
        const auto result = db_->execSqlSync(
            "SELECT event_id,user_id,peer_id FROM `friend_apply` "
            "WHERE peer_id=? ORDER BY id DESC",
            peer_id);
        std::vector<FriendApplyRecord> applies;
        for (const auto &row : result) {
            applies.push_back(FriendApplyRecord{
                FieldString(row, "event_id"),
                FieldString(row, "user_id"),
                FieldString(row, "peer_id"),
            });
        }
        return Result<std::vector<FriendApplyRecord>>::Ok(std::move(applies));
    });
}

VoidResult
OrmFriendRepository::InsertChatSession(const ChatSessionRecord &session) {
    return RunDb([&]() {
        db_->execSqlSync(
            "INSERT INTO `chat_session` "
            "(chat_session_id,chat_session_name,chat_session_type) "
            "VALUES (?,?,?)",
            session.chat_session_id, session.chat_session_name,
            static_cast<int>(session.chat_session_type));
        return VoidResult::Ok();
    });
}

VoidResult
OrmFriendRepository::InsertChatSessionMember(const std::string &session_id,
                                             const std::string &user_id) {
    return RunDb([&]() {
        db_->execSqlSync(
            "INSERT IGNORE INTO `chat_session_member` (session_id,user_id) "
            "VALUES (?,?)",
            session_id, user_id);
        return VoidResult::Ok();
    });
}

VoidResult OrmFriendRepository::InsertChatSessionMembers(
    const std::string &session_id,
    const std::vector<std::string> &user_ids) {
    if (user_ids.empty()) {
        return VoidResult::Ok();
    }
    return RunDb([&]() {
        std::string sql =
            "INSERT IGNORE INTO `chat_session_member` (session_id,user_id) "
            "VALUES ";
        for (std::size_t i = 0; i < user_ids.size(); ++i) {
            if (i > 0) {
                sql += ",";
            }
            sql += "(?,?)";
        }
        auto binder = (*db_ << sql);
        for (const auto &uid : user_ids) {
            binder << session_id << uid;
        }
        binder << drogon::orm::Mode::Blocking;
        binder >> [](const drogon::orm::Result &) {};
        binder.exec();
        return VoidResult::Ok();
    });
}

VoidResult
OrmFriendRepository::DeleteSingleChatSession(const std::string &user_id,
                                             const std::string &peer_id) {
    return RunDb([&]() {
        const auto result = db_->execSqlSync(
            "SELECT c.chat_session_id FROM `chat_session` c "
            "JOIN `chat_session_member` a ON c.chat_session_id=a.session_id "
            "JOIN `chat_session_member` b ON c.chat_session_id=b.session_id "
            "WHERE c.chat_session_type=1 AND a.user_id=? AND b.user_id=?",
            user_id, peer_id);
        for (const auto &row : result) {
            const std::string session_id = FieldString(row, "chat_session_id");
            db_->execSqlSync(
                "DELETE FROM `chat_session_member` WHERE session_id=?",
                session_id);
            db_->execSqlSync(
                "DELETE FROM `chat_session` WHERE chat_session_id=?",
                session_id);
        }
        return VoidResult::Ok();
    });
}

Result<std::vector<ChatSessionRecord>>
OrmFriendRepository::ListChatSessions(const std::string &user_id) {
    return RunDb([&]() {
        const auto result = db_->execSqlSync(
            "SELECT c.chat_session_id,c.chat_session_name,c.chat_session_type "
            "FROM `chat_session` c "
            "JOIN `chat_session_member` m ON c.chat_session_id=m.session_id "
            "WHERE m.user_id=? ORDER BY c.id DESC",
            user_id);
        std::vector<ChatSessionRecord> sessions;
        for (const auto &row : result) {
            sessions.push_back(ToChatSessionRecord(row));
        }
        return Result<std::vector<ChatSessionRecord>>::Ok(std::move(sessions));
    });
}

Result<std::vector<std::string>>
OrmFriendRepository::ListChatSessionMembers(const std::string &session_id) {
    return RunDb([&]() {
        const auto result = db_->execSqlSync(
            "SELECT user_id FROM `chat_session_member` WHERE session_id=?",
            session_id);
        std::vector<std::string> ids;
        for (const auto &row : result) {
            ids.push_back(FieldString(row, "user_id"));
        }
        return Result<std::vector<std::string>>::Ok(std::move(ids));
    });
}

Result<std::optional<std::string>>
OrmFriendRepository::FindSingleChatPeer(const std::string &session_id,
                                        const std::string &user_id) {
    return RunDb([&]() {
        const auto result =
            db_->execSqlSync("SELECT user_id FROM `chat_session_member` "
                             "WHERE session_id=? AND user_id<>? LIMIT 1",
                             session_id, user_id);
        if (result.empty()) {
            return Result<std::optional<std::string>>::Ok(std::nullopt);
        }
        return Result<std::optional<std::string>>::Ok(
            FieldString(result[0], "user_id"));
    });
}

} // namespace zchat
