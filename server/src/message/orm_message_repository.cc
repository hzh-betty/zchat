#include "message/message_repository.h"

#include <algorithm>
#include <utility>

#include <drogon/orm/SqlBinder.h>

namespace zchat {

OrmMessageRepository::OrmMessageRepository(
    std::shared_ptr<drogon::orm::DbClient> db)
    : OrmRepositoryBase(std::move(db)) {}

drogon::Task<VoidResult>
OrmMessageRepository::InsertMessageCoro(const MessageRecord &message) {
    return RunDbCoro([&]() -> drogon::Task<VoidResult> {
        co_await db_->execSqlCoro(
            "INSERT INTO `message` "
            "(message_id,session_id,user_id,message_type,create_time,content,"
            "file_id,file_name,file_size) "
            "VALUES (?,?,?,?,FROM_UNIXTIME(?),?,?,?,?)",
            message.message_id, message.session_id, message.user_id,
            message.message_type, message.create_time, message.content,
            message.file_id, message.file_name,
            static_cast<std::uint64_t>(message.file_size));
        co_return VoidResult::Ok();
    });
}

drogon::Task<Result<std::vector<MessageRecord>>>
OrmMessageRepository::ListRecentMessagesCoro(const std::string &session_id,
                                             int count) {
    return RunDbCoro([&]() -> drogon::Task<Result<std::vector<MessageRecord>>> {
        const auto result = co_await db_->execSqlCoro(
            "SELECT message_id,session_id,user_id,message_type,"
            "UNIX_TIMESTAMP(create_time) AS create_time,content,file_id,"
            "file_name,file_size FROM `message` WHERE session_id=? "
            "ORDER BY create_time DESC,id DESC LIMIT ?",
            session_id, count);
        std::vector<MessageRecord> messages;
        for (const auto &row : result) {
            messages.push_back(ToMessageRecord(row));
        }
        std::reverse(messages.begin(), messages.end());
        co_return Result<std::vector<MessageRecord>>::Ok(std::move(messages));
    });
}

drogon::Task<Result<std::vector<MessageRecord>>>
OrmMessageRepository::ListLastMessagesForSessionsCoro(
    const std::vector<std::string> &session_ids) {
    if (session_ids.empty()) {
        co_return Result<std::vector<MessageRecord>>::Ok({});
    }
    co_return co_await RunDbCoro(
        [&]() -> drogon::Task<Result<std::vector<MessageRecord>>> {
            // session_id 由 CSPRNG 生成，安全拼接
            std::string placeholders;
            for (std::size_t i = 0; i < session_ids.size(); ++i) {
                if (i > 0)
                    placeholders += ",";
                placeholders += "'" + session_ids[i] + "'";
            }
            const std::string sql =
                "SELECT m.message_id,m.session_id,m.user_id,m.message_type,"
                "UNIX_TIMESTAMP(m.create_time) AS create_time,m.content,"
                "m.file_id,m.file_name,m.file_size FROM `message` m "
                "INNER JOIN (SELECT session_id,MAX(id) AS max_id FROM "
                "`message` WHERE session_id IN (" +
                placeholders + ") GROUP BY session_id) t ON m.id=t.max_id";
            const auto result = co_await db_->execSqlCoro(sql);
            std::vector<MessageRecord> messages;
            for (const auto &row : result) {
                messages.push_back(ToMessageRecord(row));
            }
            co_return Result<std::vector<MessageRecord>>::Ok(
                std::move(messages));
        });
}

drogon::Task<Result<std::vector<MessageRecord>>>
OrmMessageRepository::ListMessagesByTimeCoro(
    const std::string &session_id, std::int64_t start_time,
    std::int64_t end_time, int max_count,
    std::optional<std::string> before_msg_id) {
    co_return co_await RunDbCoro(
        [&]() -> drogon::Task<Result<std::vector<MessageRecord>>> {
            const bool has_cursor =
                before_msg_id.has_value() && !before_msg_id.value().empty();
            std::int64_t cursor_time = 0;
            std::int64_t cursor_id = 0;
            if (has_cursor) {
                const auto cursor = co_await db_->execSqlCoro(
                    "SELECT UNIX_TIMESTAMP(create_time) AS create_time,id FROM "
                    "`message` WHERE message_id=? LIMIT 1",
                    before_msg_id.value());
                if (cursor.empty()) {
                    co_return Result<std::vector<MessageRecord>>::Ok({});
                }
                cursor_time = FieldInt64(cursor[0], "create_time");
                cursor_id = FieldInt64(cursor[0], "id");
            }

            std::string sql =
                "SELECT message_id,session_id,user_id,message_type,"
                "UNIX_TIMESTAMP(create_time) AS create_time,content,file_id,"
                "file_name,file_size FROM `message` WHERE session_id=? "
                "AND create_time>=FROM_UNIXTIME(?) AND "
                "create_time<=FROM_UNIXTIME(?)";
            if (has_cursor) {
                sql += " AND (create_time,id) < (FROM_UNIXTIME(?),?)";
            }
            sql += " ORDER BY create_time DESC,id DESC LIMIT ?";

            if (has_cursor) {
                const auto result = co_await db_->execSqlCoro(
                    sql, session_id, start_time, end_time, cursor_time,
                    cursor_id, max_count);
                std::vector<MessageRecord> messages;
                for (const auto &row : result) {
                    messages.push_back(ToMessageRecord(row));
                }
                std::reverse(messages.begin(), messages.end());
                co_return Result<std::vector<MessageRecord>>::Ok(
                    std::move(messages));
            } else {
                const auto result = co_await db_->execSqlCoro(
                    sql, session_id, start_time, end_time, max_count);
                std::vector<MessageRecord> messages;
                for (const auto &row : result) {
                    messages.push_back(ToMessageRecord(row));
                }
                std::reverse(messages.begin(), messages.end());
                co_return Result<std::vector<MessageRecord>>::Ok(
                    std::move(messages));
            }
        });
}

drogon::Task<Result<std::optional<MessageRecord>>>
OrmMessageRepository::LastMessageCoro(const std::string &session_id) {
    auto messages = co_await ListRecentMessagesCoro(session_id, 1);
    if (!messages.ok()) {
        co_return Result<std::optional<MessageRecord>>::Fail(
            messages.error().message);
    }
    if (messages.value().empty()) {
        co_return Result<std::optional<MessageRecord>>::Ok(std::nullopt);
    }
    co_return Result<std::optional<MessageRecord>>::Ok(
        messages.value().front());
}

} // namespace zchat
