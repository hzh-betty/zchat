#include "file/file_repository.h"

#include <utility>

namespace zchat {

OrmFileRepository::OrmFileRepository(std::shared_ptr<drogon::orm::DbClient> db)
    : OrmRepositoryBase(std::move(db)) {}

VoidResult OrmFileRepository::PutFile(const FileRecord &file) {
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

Result<std::optional<FileRecord>>
OrmFileRepository::GetFile(const std::string &file_id) {
    return RunDb([&]() {
        const auto result = db_->execSqlSync(
            "SELECT file_id,file_name,file_size,file_content FROM `file_store` "
            "WHERE file_id=? LIMIT 1",
            file_id);
        if (result.empty()) {
            return Result<std::optional<FileRecord>>::Ok(std::nullopt);
        }
        return Result<std::optional<FileRecord>>::Ok(ToFileRecord(result[0]));
    });
}

} // namespace zchat
