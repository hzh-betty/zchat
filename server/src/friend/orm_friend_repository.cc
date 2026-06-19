#include "friend/friend_repository.h"

#include <utility>

#include <drogon/orm/DbClient.h>
#include <drogon/orm/SqlBinder.h>

namespace zchat {

OrmFriendRepository::OrmFriendRepository(
    std::shared_ptr<drogon::orm::DbClient> db)
    : OrmRepositoryBase(std::move(db)) {}

drogon::Task<VoidResult>
OrmFriendRepository::InsertRelationCoro(const std::string &user_id,
                                        const std::string &peer_id) {
    return RunDbCoro([&]() -> drogon::Task<VoidResult> {
        co_await db_->execSqlCoro(
            "INSERT IGNORE INTO `relation` (user_id,peer_id) VALUES (?,?)",
            user_id, peer_id);
        co_await db_->execSqlCoro(
            "INSERT IGNORE INTO `relation` (user_id,peer_id) VALUES (?,?)",
            peer_id, user_id);
        co_return VoidResult::Ok();
    });
}

drogon::Task<VoidResult>
OrmFriendRepository::DeleteRelationCoro(const std::string &user_id,
                                        const std::string &peer_id) {
    return RunDbCoro([&]() -> drogon::Task<VoidResult> {
        co_await db_->execSqlCoro(
            "DELETE FROM `relation` WHERE (user_id=? AND peer_id=?) OR "
            "(user_id=? AND peer_id=?)",
            user_id, peer_id, peer_id, user_id);
        co_return VoidResult::Ok();
    });
}

drogon::Task<Result<bool>>
OrmFriendRepository::RelationExistsCoro(const std::string &user_id,
                                        const std::string &peer_id) {
    return RunDbCoro([&]() -> drogon::Task<Result<bool>> {
        const auto result = co_await db_->execSqlCoro(
            "SELECT id FROM `relation` WHERE user_id=? AND peer_id=? LIMIT 1",
            user_id, peer_id);
        co_return Result<bool>::Ok(!result.empty());
    });
}

drogon::Task<Result<std::vector<std::string>>>
OrmFriendRepository::ListExistingPeersCoro(
    const std::string &user_id, const std::vector<std::string> &peer_ids) {
    if (peer_ids.empty()) {
        co_return Result<std::vector<std::string>>::Ok({});
    }
    co_return co_await RunDbCoro(
        [&]() -> drogon::Task<Result<std::vector<std::string>>> {
            std::string sql = "SELECT peer_id FROM `relation` WHERE user_id='";
            sql += user_id;
            sql += "' AND peer_id IN ('";
            for (std::size_t i = 0; i < peer_ids.size(); ++i) {
                if (i > 0)
                    sql += "','";
                sql += peer_ids[i];
            }
            sql += "')";
            const auto result = co_await db_->execSqlCoro(sql);
            std::vector<std::string> existing;
            for (const auto &row : result) {
                existing.push_back(FieldString(row, "peer_id"));
            }
            co_return Result<std::vector<std::string>>::Ok(std::move(existing));
        });
}

drogon::Task<Result<std::vector<std::string>>>
OrmFriendRepository::ListFriendIdsCoro(const std::string &user_id) {
    return RunDbCoro([&]() -> drogon::Task<Result<std::vector<std::string>>> {
        const auto result = co_await db_->execSqlCoro(
            "SELECT peer_id FROM `relation` WHERE user_id=? ORDER BY id DESC",
            user_id);
        std::vector<std::string> ids;
        for (const auto &row : result) {
            ids.push_back(FieldString(row, "peer_id"));
        }
        co_return Result<std::vector<std::string>>::Ok(std::move(ids));
    });
}

drogon::Task<VoidResult>
OrmFriendRepository::InsertFriendApplyCoro(const FriendApplyRecord &apply) {
    return RunDbCoro([&]() -> drogon::Task<VoidResult> {
        co_await db_->execSqlCoro("INSERT INTO `friend_apply` "
                                  "(event_id,user_id,peer_id) VALUES (?,?,?)",
                                  apply.event_id, apply.user_id, apply.peer_id);
        co_return VoidResult::Ok();
    });
}

drogon::Task<VoidResult>
OrmFriendRepository::DeleteFriendApplyCoro(const std::string &user_id,
                                           const std::string &peer_id) {
    return RunDbCoro([&]() -> drogon::Task<VoidResult> {
        co_await db_->execSqlCoro(
            "DELETE FROM `friend_apply` WHERE user_id=? AND peer_id=?", user_id,
            peer_id);
        co_return VoidResult::Ok();
    });
}

drogon::Task<Result<bool>>
OrmFriendRepository::FriendApplyExistsCoro(const std::string &user_id,
                                           const std::string &peer_id) {
    return RunDbCoro([&]() -> drogon::Task<Result<bool>> {
        const auto result = co_await db_->execSqlCoro(
            "SELECT id FROM `friend_apply` WHERE user_id=? "
            "AND peer_id=? LIMIT 1",
            user_id, peer_id);
        co_return Result<bool>::Ok(!result.empty());
    });
}

drogon::Task<Result<std::vector<FriendApplyRecord>>>
OrmFriendRepository::ListPendingAppliesCoro(const std::string &peer_id) {
    return RunDbCoro(
        [&]() -> drogon::Task<Result<std::vector<FriendApplyRecord>>> {
            const auto result = co_await db_->execSqlCoro(
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
            co_return Result<std::vector<FriendApplyRecord>>::Ok(
                std::move(applies));
        });
}

drogon::Task<VoidResult>
OrmFriendRepository::InsertChatSessionCoro(const ChatSessionRecord &session) {
    return RunDbCoro([&]() -> drogon::Task<VoidResult> {
        co_await db_->execSqlCoro(
            "INSERT INTO `chat_session` "
            "(chat_session_id,chat_session_name,chat_session_type) "
            "VALUES (?,?,?)",
            session.chat_session_id, session.chat_session_name,
            static_cast<int>(session.chat_session_type));
        co_return VoidResult::Ok();
    });
}

drogon::Task<VoidResult>
OrmFriendRepository::InsertChatSessionMemberCoro(const std::string &session_id,
                                                 const std::string &user_id) {
    return RunDbCoro([&]() -> drogon::Task<VoidResult> {
        co_await db_->execSqlCoro(
            "INSERT IGNORE INTO `chat_session_member` (session_id,user_id) "
            "VALUES (?,?)",
            session_id, user_id);
        co_return VoidResult::Ok();
    });
}

drogon::Task<VoidResult> OrmFriendRepository::InsertChatSessionMembersCoro(
    const std::string &session_id, const std::vector<std::string> &user_ids) {
    if (user_ids.empty()) {
        co_return VoidResult::Ok();
    }
    co_return co_await RunDbCoro([&]() -> drogon::Task<VoidResult> {
        for (const auto &uid : user_ids) {
            co_await db_->execSqlCoro(
                "INSERT IGNORE INTO `chat_session_member` "
                "(session_id,user_id) VALUES (?,?)",
                session_id, uid);
        }
        co_return VoidResult::Ok();
    });
}

drogon::Task<VoidResult>
OrmFriendRepository::DeleteSingleChatSessionCoro(const std::string &user_id,
                                                 const std::string &peer_id) {
    return RunDbCoro([&]() -> drogon::Task<VoidResult> {
        const auto result = co_await db_->execSqlCoro(
            "SELECT c.chat_session_id FROM `chat_session` c "
            "JOIN `chat_session_member` a ON c.chat_session_id=a.session_id "
            "JOIN `chat_session_member` b ON c.chat_session_id=b.session_id "
            "WHERE c.chat_session_type=1 AND a.user_id=? AND b.user_id=?",
            user_id, peer_id);
        for (const auto &row : result) {
            const std::string session_id = FieldString(row, "chat_session_id");
            co_await db_->execSqlCoro(
                "DELETE FROM `chat_session_member` WHERE session_id=?",
                session_id);
            co_await db_->execSqlCoro(
                "DELETE FROM `chat_session` WHERE chat_session_id=?",
                session_id);
        }
        co_return VoidResult::Ok();
    });
}

drogon::Task<Result<std::vector<ChatSessionRecord>>>
OrmFriendRepository::ListChatSessionsCoro(const std::string &user_id) {
    return RunDbCoro(
        [&]() -> drogon::Task<Result<std::vector<ChatSessionRecord>>> {
            const auto result = co_await db_->execSqlCoro(
                "SELECT c.chat_session_id,c.chat_session_name,"
                "c.chat_session_type FROM `chat_session` c "
                "JOIN `chat_session_member` m ON "
                "c.chat_session_id=m.session_id "
                "WHERE m.user_id=? ORDER BY c.id DESC",
                user_id);
            std::vector<ChatSessionRecord> sessions;
            for (const auto &row : result) {
                sessions.push_back(ToChatSessionRecord(row));
            }
            co_return Result<std::vector<ChatSessionRecord>>::Ok(
                std::move(sessions));
        });
}

drogon::Task<Result<std::vector<std::string>>>
OrmFriendRepository::ListChatSessionMembersCoro(const std::string &session_id) {
    return RunDbCoro([&]() -> drogon::Task<Result<std::vector<std::string>>> {
        const auto result = co_await db_->execSqlCoro(
            "SELECT user_id FROM `chat_session_member` WHERE session_id=?",
            session_id);
        std::vector<std::string> ids;
        for (const auto &row : result) {
            ids.push_back(FieldString(row, "user_id"));
        }
        co_return Result<std::vector<std::string>>::Ok(std::move(ids));
    });
}

drogon::Task<Result<std::optional<std::string>>>
OrmFriendRepository::FindSingleChatPeerCoro(const std::string &session_id,
                                            const std::string &user_id) {
    return RunDbCoro([&]() -> drogon::Task<Result<std::optional<std::string>>> {
        const auto result = co_await db_->execSqlCoro(
            "SELECT user_id FROM `chat_session_member` "
            "WHERE session_id=? AND user_id<>? LIMIT 1",
            session_id, user_id);
        if (result.empty()) {
            co_return Result<std::optional<std::string>>::Ok(std::nullopt);
        }
        co_return Result<std::optional<std::string>>::Ok(
            FieldString(result[0], "user_id"));
    });
}

} // namespace zchat
