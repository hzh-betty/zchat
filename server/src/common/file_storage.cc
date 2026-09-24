#include "common/file_storage.h"

#include "common/resource_limits.h"

namespace zchat {
namespace {

struct CommitAwaiter : drogon::CallbackAwaiter<bool> {
    explicit CommitAwaiter(
        std::shared_ptr<drogon::orm::Transaction> transaction)
        : transaction_(std::move(transaction)) {}

    void await_suspend(std::coroutine_handle<> handle) {
        transaction_->setCommitCallback([this, handle](bool committed) {
            setValue(committed);
            handle.resume();
        });
        transaction_.reset();
    }

    std::shared_ptr<drogon::orm::Transaction> transaction_;
};

} // namespace

drogon::Task<VoidResult>
StoreFileCoro(std::shared_ptr<drogon::orm::DbClient> db, FileRecord file,
              bool replace_avatar) {
    using namespace resource_limits;
    if (file.owner_user_id.empty() || file.owner_user_id.size() > 64 ||
        file.file_content.size() >
            (replace_avatar ? kAvatarBytes : kFileBytes)) {
        co_return VoidResult::Fail(
            AppError::WithCode(ErrorCode::kFileStorageRejected,
                               "invalid file owner or file too large"));
    }
    co_return co_await RunDbCoro([&]() -> drogon::Task<VoidResult> {
        auto transaction = co_await db->newTransactionCoro();
        if (!transaction) {
            co_return VoidResult::Fail(
                common_errors::DatabaseOperationFailed());
        }
        try {
            // Serialize quota checks on the existing indexed table before any
            // aggregate read (MySQL's default REPEATABLE READ isolation).
            co_await transaction->execSqlCoro(
                "SELECT file_id FROM file_store WHERE file_id >= '' FOR "
                "UPDATE");
            std::uint64_t old_bytes = 0;
            bool overwrite = false;
            if (replace_avatar) {
                const auto user = co_await transaction->execSqlCoro(
                    "SELECT avatar_id FROM `user` WHERE user_id=? FOR UPDATE",
                    file.owner_user_id);
                if (user.empty()) {
                    transaction->rollback();
                    co_return VoidResult::Fail(AppError::WithCode(
                        ErrorCode::kNotFound, "avatar owner not found"));
                }
                const auto old = co_await transaction->execSqlCoro(
                    "SELECT file_id,OCTET_LENGTH(file_content) AS bytes "
                    "FROM file_store WHERE file_id=? AND owner_user_id=? "
                    "AND chat_session_id IS NULL FOR UPDATE",
                    FieldString(user[0], "avatar_id"), file.owner_user_id);
                if (!old.empty()) {
                    file.file_id = FieldString(old[0], "file_id");
                    old_bytes = old[0]["bytes"].as<std::uint64_t>();
                    overwrite = true;
                }
            }
            // Derive usage from the stored contents so existing records do not
            // require a schema or data migration.
            const auto usage = co_await transaction->execSqlCoro(
                "SELECT COUNT(*) AS files, "
                "COALESCE(SUM(OCTET_LENGTH(file_content)),0) AS bytes, "
                "COALESCE(SUM(owner_user_id=?),0) AS user_files, "
                "COALESCE(SUM(CASE WHEN owner_user_id=? "
                "THEN OCTET_LENGTH(file_content) ELSE 0 END),0) AS user_bytes "
                "FROM file_store",
                file.owner_user_id, file.owner_user_id);
            const auto bytes =
                static_cast<std::uint64_t>(file.file_content.size());
            const auto added_files = overwrite ? 0U : 1U;
            const auto &row = usage[0];
            if (row["bytes"].as<std::uint64_t>() - old_bytes + bytes >
                    kGlobalBytes ||
                row["user_bytes"].as<std::uint64_t>() - old_bytes + bytes >
                    kUserBytes ||
                row["files"].as<std::uint64_t>() + added_files > kGlobalFiles ||
                row["user_files"].as<std::uint64_t>() + added_files >
                    kUserFiles) {
                transaction->rollback();
                co_return VoidResult::Fail(
                    AppError::WithCode(ErrorCode::kFileStorageRejected,
                                       "file storage quota exceeded"));
            }
            if (overwrite) {
                co_await transaction->execSqlCoro(
                    "UPDATE file_store SET file_content=?,file_size=? WHERE "
                    "file_id=?",
                    file.file_content, bytes, file.file_id);
            } else {
                co_await transaction->execSqlCoro(
                    "INSERT INTO file_store "
                    "(file_id,file_name,file_size,file_content,"
                    "owner_user_id,chat_session_id) VALUES "
                    "(?,?,?,?,?,NULLIF(?, ''))",
                    file.file_id, file.file_name, bytes, file.file_content,
                    file.owner_user_id, file.chat_session_id);
            }
            if (replace_avatar) {
                co_await transaction->execSqlCoro(
                    "UPDATE `user` SET avatar_id=? WHERE user_id=?",
                    file.file_id, file.owner_user_id);
            }
        } catch (...) {
            transaction->rollback();
            throw;
        }
        if (!(co_await CommitAwaiter(std::move(transaction)))) {
            co_return VoidResult::Fail(
                common_errors::DatabaseOperationFailed());
        }
        co_return VoidResult::Ok();
    });
}

} // namespace zchat
