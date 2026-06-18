#ifndef ZCHAT_SERVER_SRC_COMMON_SESSION_STORE_H_
#define ZCHAT_SERVER_SRC_COMMON_SESSION_STORE_H_

#include "common/noncopyable.h"

#include <memory>
#include <optional>
#include <string>

#include <drogon/nosql/RedisClient.h>
#include <drogon/utils/coroutine.h>

#include "common/result.h"

namespace zchat {

class SessionStore : public NonCopyable {
  public:
    explicit SessionStore(drogon::nosql::RedisClientPtr redis);

    ~SessionStore() = default;

    drogon::Task<Result<std::optional<std::string>>>
    GetUserIdCoro(const std::string &session_id);
    drogon::Task<VoidResult> SaveSessionCoro(const std::string &session_id,
                                             const std::string &user_id);
    drogon::Task<VoidResult> RemoveSessionCoro(const std::string &session_id);
    drogon::Task<VoidResult> SetOnlineCoro(const std::string &user_id);
    drogon::Task<Result<bool>>
    SetOnlineIfAbsentCoro(const std::string &user_id);
    drogon::Task<VoidResult> RefreshOnlineCoro(const std::string &user_id);
    drogon::Task<VoidResult> SetOfflineCoro(const std::string &user_id);
    drogon::Task<Result<bool>> IsOnlineCoro(const std::string &user_id);
    drogon::Task<VoidResult>
    SaveVerifyCodeCoro(const std::string &verify_code_id,
                       const std::string &verify_code);
    drogon::Task<Result<std::optional<std::string>>>
    GetVerifyCodeCoro(const std::string &verify_code_id);
    drogon::Task<VoidResult>
    RemoveVerifyCodeCoro(const std::string &verify_code_id);
    drogon::Task<Result<int>> RecordLoginFailCoro(const std::string &user_id);
    drogon::Task<Result<bool>> IsAccountLockedCoro(const std::string &user_id);
    drogon::Task<VoidResult> ClearLoginFailCoro(const std::string &user_id);

  private:
    drogon::nosql::RedisClientPtr redis_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_COMMON_SESSION_STORE_H_
