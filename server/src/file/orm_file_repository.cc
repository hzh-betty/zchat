#include "file/file_repository.h"

#include <utility>

#include <drogon/orm/DbClient.h>
#include <drogon/orm/SqlBinder.h>

namespace zchat {

OrmFileRepository::OrmFileRepository(std::shared_ptr<drogon::orm::DbClient> db)
    : OrmRepositoryBase(std::move(db)) {}

drogon::Task<VoidResult>
OrmFileRepository::PutFileCoro(const FileRecord &file) {
    return RunDbCoro([&]() -> drogon::Task<VoidResult> {
        co_await db_->execSqlCoro(
            "INSERT INTO `file_store` "
            "(file_id,file_name,file_size,file_content,owner_user_id,"
            "chat_session_id) VALUES (?,?,?,?,NULLIF(?, ''),NULLIF(?, '')) "
            "ON DUPLICATE KEY UPDATE file_name=VALUES(file_name),"
            "file_size=VALUES(file_size),file_content=VALUES(file_content),"
            "owner_user_id=VALUES(owner_user_id),"
            "chat_session_id=VALUES(chat_session_id)",
            file.file_id, file.file_name,
            static_cast<std::uint64_t>(file.file_size), file.file_content,
            file.owner_user_id, file.chat_session_id);
        co_return VoidResult::Ok();
    });
}

drogon::Task<Result<std::optional<FileRecord>>>
OrmFileRepository::GetFileCoro(const std::string &file_id) {
    return RunDbCoro([&]() -> drogon::Task<Result<std::optional<FileRecord>>> {
        const auto result = co_await db_->execSqlCoro(
            "SELECT file_id,file_name,file_size,file_content,owner_user_id,"
            "chat_session_id FROM `file_store` WHERE file_id=? LIMIT 1",
            file_id);
        if (result.empty()) {
            co_return Result<std::optional<FileRecord>>::Ok(std::nullopt);
        }
        co_return Result<std::optional<FileRecord>>::Ok(
            ToFileRecord(result[0]));
    });
}

drogon::Task<Result<std::vector<FileRecord>>>
OrmFileRepository::FindFilesByIdsCoro(
    const std::vector<std::string> &file_ids) {
    if (file_ids.empty()) {
        co_return Result<std::vector<FileRecord>>::Ok({});
    }
    co_return co_await RunDbCoro(
        [&]() -> drogon::Task<Result<std::vector<FileRecord>>> {
            std::string placeholders;
            for (std::size_t i = 0; i < file_ids.size(); ++i) {
                if (i > 0)
                    placeholders += ",";
                placeholders += "?";
            }
            const std::string sql =
                "SELECT file_id,file_name,file_size,file_content,"
                "owner_user_id,chat_session_id FROM `file_store` "
                "WHERE file_id IN (" +
                placeholders + ")";
            const auto result = co_await db_->execSqlCoro(sql, file_ids);
            std::vector<FileRecord> files;
            for (const auto &row : result) {
                files.push_back(ToFileRecord(row));
            }
            co_return Result<std::vector<FileRecord>>::Ok(std::move(files));
        });
}

} // namespace zchat
