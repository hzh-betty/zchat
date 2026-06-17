#include "message/message_repository.h"

#include <algorithm>
#include <utility>

#include <drogon/orm/SqlBinder.h>

namespace zchat {

OrmMessageRepository::OrmMessageRepository(
    std::shared_ptr<drogon::orm::DbClient> db)
    : OrmRepositoryBase(std::move(db)) {}

VoidResult OrmMessageRepository::InsertMessage(const MessageRecord &message) {
    return RunDb([&]() {
        db_->execSqlSync(
            "INSERT INTO `message` "
            "(message_id,session_id,user_id,message_type,create_time,content,"
            "file_id,file_name,file_size) "
            "VALUES (?,?,?,?,FROM_UNIXTIME(?),?,?,?,?)",
            message.message_id, message.session_id, message.user_id,
            message.message_type, message.create_time, message.content,
            message.file_id, message.file_name,
            static_cast<std::uint64_t>(message.file_size));
        return VoidResult::Ok();
    });
}

Result<std::vector<MessageRecord>>
OrmMessageRepository::ListRecentMessages(const std::string &session_id,
                                         int count) {
    return RunDb([&]() {
        const auto result = db_->execSqlSync(
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
        return Result<std::vector<MessageRecord>>::Ok(std::move(messages));
    });
}

Result<std::vector<MessageRecord>> OrmMessageRepository::ListMessagesByTime(
    const std::string &session_id, std::int64_t start_time,
    std::int64_t end_time, int max_count,
    std::optional<std::string> before_msg_id) {
    return RunDb([&]() -> Result<std::vector<MessageRecord>> {
        const bool has_cursor =
            before_msg_id.has_value() && !before_msg_id.value().empty();
        std::int64_t cursor_time = 0;
        std::int64_t cursor_id = 0;
        if (has_cursor) {
            const auto cursor = db_->execSqlSync(
                "SELECT UNIX_TIMESTAMP(create_time) AS create_time,id FROM "
                "`message` WHERE message_id=? LIMIT 1",
                before_msg_id.value());
            if (cursor.empty()) {
                return Result<std::vector<MessageRecord>>::Ok({});
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

        auto binder = (*db_ << sql);
        binder << session_id << start_time << end_time;
        if (has_cursor) {
            binder << cursor_time << cursor_id;
        }
        binder << max_count;
        binder << drogon::orm::Mode::Blocking;
        drogon::orm::Result result(nullptr);
        binder >> [&result](const drogon::orm::Result &r) { result = r; };
        binder.exec();

        std::vector<MessageRecord> messages;
        for (const auto &row : result) {
            messages.push_back(ToMessageRecord(row));
        }
        std::reverse(messages.begin(), messages.end());
        return Result<std::vector<MessageRecord>>::Ok(std::move(messages));
    });
}

Result<std::optional<MessageRecord>>
OrmMessageRepository::LastMessage(const std::string &session_id) {
    auto messages = ListRecentMessages(session_id, 1);
    if (!messages.ok()) {
        return Result<std::optional<MessageRecord>>::Fail(
            messages.error().message);
    }
    if (messages.value().empty()) {
        return Result<std::optional<MessageRecord>>::Ok(std::nullopt);
    }
    return Result<std::optional<MessageRecord>>::Ok(messages.value().front());
}

} // namespace zchat
