#include "message/message_repository.h"

#include <algorithm>
#include <utility>

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

Result<std::vector<MessageRecord>>
OrmMessageRepository::ListMessagesByTime(const std::string &session_id,
                                         std::int64_t start_time,
                                         std::int64_t end_time) {
    return RunDb([&]() {
        const auto result = db_->execSqlSync(
            "SELECT message_id,session_id,user_id,message_type,"
            "UNIX_TIMESTAMP(create_time) AS create_time,content,file_id,"
            "file_name,file_size FROM `message` WHERE session_id=? "
            "AND create_time>=FROM_UNIXTIME(?) AND "
            "create_time<=FROM_UNIXTIME(?) "
            "ORDER BY create_time ASC,id ASC",
            session_id, start_time, end_time);
        std::vector<MessageRecord> messages;
        for (const auto &row : result) {
            messages.push_back(ToMessageRecord(row));
        }
        return Result<std::vector<MessageRecord>>::Ok(std::move(messages));
    });
}

Result<std::vector<MessageRecord>>
OrmMessageRepository::SearchMessages(const std::string &session_id,
                                     const std::string &keyword) {
    return RunDb([&]() {
        const std::string like = "%" + keyword + "%";
        const auto result = db_->execSqlSync(
            "SELECT message_id,session_id,user_id,message_type,"
            "UNIX_TIMESTAMP(create_time) AS create_time,content,file_id,"
            "file_name,file_size FROM `message` WHERE session_id=? "
            "AND message_type=0 AND content LIKE ? ORDER BY create_time ASC,id "
            "ASC",
            session_id, like);
        std::vector<MessageRecord> messages;
        for (const auto &row : result) {
            messages.push_back(ToMessageRecord(row));
        }
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
