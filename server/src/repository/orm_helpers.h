#ifndef ZCHAT_SERVER_SRC_REPOSITORY_ORM_HELPERS_H_
#define ZCHAT_SERVER_SRC_REPOSITORY_ORM_HELPERS_H_

#include "common/noncopyable.h"

#include <cstdint>
#include <exception>
#include <memory>
#include <string>
#include <utility>

#include <drogon/orm/DbClient.h>
#include <drogon/orm/Exception.h>
#include <drogon/orm/Row.h>

#include "common/domain_records.h"
#include "common/result.h"

namespace zchat {

class OrmRepositoryBase : public NonCopyable {
  public:
    explicit OrmRepositoryBase(std::shared_ptr<drogon::orm::DbClient> db);

    virtual ~OrmRepositoryBase() = default;

  protected:
    std::shared_ptr<drogon::orm::DbClient> db_;
};

std::string FieldString(const drogon::orm::Row &row, const char *name);
std::int64_t FieldInt64(const drogon::orm::Row &row, const char *name);
UserRecord ToUserRecord(const drogon::orm::Row &row);
MessageRecord ToMessageRecord(const drogon::orm::Row &row);
ChatSessionRecord ToChatSessionRecord(const drogon::orm::Row &row);
FileRecord ToFileRecord(const drogon::orm::Row &row);

template <typename Func> auto RunDb(Func function) -> decltype(function()) {
    try {
        return function();
    } catch (const drogon::orm::DrogonDbException &error) {
        using ReturnType = decltype(function());
        AppError app_error =
            AppError::WithCode(ErrorCode::kDatabaseError, "database operation failed");
        app_error.detail = error.base().what();
        return ReturnType::Fail(std::move(app_error));
    } catch (const std::exception &error) {
        using ReturnType = decltype(function());
        AppError app_error =
            AppError::WithCode(ErrorCode::kUnknown, "internal service error");
        app_error.detail = error.what();
        return ReturnType::Fail(std::move(app_error));
    }
}

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_REPOSITORY_ORM_HELPERS_H_
