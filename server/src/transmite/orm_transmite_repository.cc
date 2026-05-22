#include "transmite/transmite_repository.h"

#include <utility>

namespace zchat {

OrmTransmiteRepository::OrmTransmiteRepository(
    std::shared_ptr<drogon::orm::DbClient> db)
    : OrmRepositoryBase(std::move(db)) {}

VoidResult OrmTransmiteRepository::InsertMessage(const MessageRecord &message) {
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

VoidResult OrmTransmiteRepository::PutFile(const FileRecord &file) {
    return RunDb([&]() {
        db_->execSqlSync(
            "INSERT INTO `file_store` "
            "(file_id,file_name,file_size,file_content) VALUES (?,?,?,?) "
            "ON DUPLICATE KEY UPDATE file_name=VALUES(file_name),"
            "file_size=VALUES(file_size),file_content=VALUES(file_content)",
            file.file_id, file.file_name,
            static_cast<std::uint64_t>(file.file_size), file.file_content);
        return VoidResult::Ok();
    });
}

Result<std::vector<std::string>>
OrmTransmiteRepository::ListChatSessionMembers(const std::string &session_id) {
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

} // namespace zchat
