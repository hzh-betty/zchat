#include "common/orm_helpers.h"

#include <utility>

namespace zchat {

OrmRepositoryBase::OrmRepositoryBase(std::shared_ptr<drogon::orm::DbClient> db)
    : db_(std::move(db)) {}

std::string FieldString(const drogon::orm::Row &row, const char *name) {
    if (row[name].isNull()) {
        return std::string();
    }
    return row[name].as<std::string>();
}

std::int64_t FieldInt64(const drogon::orm::Row &row, const char *name) {
    if (row[name].isNull()) {
        return 0;
    }
    return row[name].as<std::int64_t>();
}

UserRecord ToUserRecord(const drogon::orm::Row &row) {
    UserRecord user;
    user.user_id = FieldString(row, "user_id");
    user.nickname = FieldString(row, "nickname");
    user.description = FieldString(row, "description");
    user.password = FieldString(row, "password");
    user.password_hash_algo = FieldString(row, "password_hash_algo");
    user.phone = FieldString(row, "phone");
    user.avatar_id = FieldString(row, "avatar_id");
    return user;
}

MessageRecord ToMessageRecord(const drogon::orm::Row &row) {
    MessageRecord message;
    message.message_id = FieldString(row, "message_id");
    message.session_id = FieldString(row, "session_id");
    message.user_id = FieldString(row, "user_id");
    message.message_type = static_cast<int>(FieldInt64(row, "message_type"));
    message.create_time = FieldInt64(row, "create_time");
    message.content = FieldString(row, "content");
    message.file_id = FieldString(row, "file_id");
    message.file_name = FieldString(row, "file_name");
    message.file_size =
        static_cast<std::uint64_t>(FieldInt64(row, "file_size"));
    return message;
}

ChatSessionRecord ToChatSessionRecord(const drogon::orm::Row &row) {
    ChatSessionRecord session;
    session.chat_session_id = FieldString(row, "chat_session_id");
    session.chat_session_name = FieldString(row, "chat_session_name");
    session.chat_session_type =
        static_cast<ChatSessionType>(FieldInt64(row, "chat_session_type"));
    return session;
}

FileRecord ToFileRecord(const drogon::orm::Row &row) {
    FileRecord file;
    file.file_id = FieldString(row, "file_id");
    file.file_name = FieldString(row, "file_name");
    file.file_size = static_cast<std::uint64_t>(FieldInt64(row, "file_size"));
    file.file_content = FieldString(row, "file_content");
    file.owner_user_id = FieldString(row, "owner_user_id");
    file.chat_session_id = FieldString(row, "chat_session_id");
    return file;
}

} // namespace zchat
