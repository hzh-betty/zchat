#ifndef ZCHAT_SERVER_SRC_COMMON_ORM_HELPERS_H_
#define ZCHAT_SERVER_SRC_COMMON_ORM_HELPERS_H_

#include "common/noncopyable.h"

#include <cstdint>
#include <exception>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include <drogon/orm/DbClient.h>
#include <drogon/orm/Exception.h>
#include <drogon/orm/Row.h>
#include <drogon/utils/coroutine.h>

#include "common/common_errors.h"
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

template <typename Func>
drogon::Task<std::decay_t<decltype(std::declval<std::invoke_result_t<Func>>()
                                       .operator co_await()
                                       .await_resume())>>
RunDbCoro(Func function) {
    using ReturnType =
        std::decay_t<decltype(std::declval<std::invoke_result_t<Func>>()
                                  .operator co_await()
                                  .await_resume())>;
    try {
        co_return co_await function();
    } catch (const drogon::orm::DrogonDbException &error) {
        AppError app_error = common_errors::DatabaseOperationFailed();
        app_error.detail = error.base().what();
        co_return ReturnType::Fail(std::move(app_error));
    } catch (const std::exception &error) {
        AppError app_error = common_errors::InternalServiceError();
        app_error.detail = error.what();
        co_return ReturnType::Fail(std::move(app_error));
    }
}

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_COMMON_ORM_HELPERS_H_
