#ifndef ZCHAT_SERVER_SRC_COMMON_SESSION_STORE_H_
#define ZCHAT_SERVER_SRC_COMMON_SESSION_STORE_H_

#include "common/noncopyable.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include <drogon/nosql/RedisClient.h>

#include "common/result.h"

namespace zchat {

class SessionStore : public NonCopyable {
  public:
    explicit SessionStore(drogon::nosql::RedisClientPtr redis);

    ~SessionStore() = default;

    VoidResult SaveSession(const std::string &session_id,
                           const std::string &user_id);
    Result<std::optional<std::string>> GetUserId(const std::string &session_id);
    void GetUserIdAsync(
        const std::string &session_id,
        std::function<void(Result<std::optional<std::string>>)> &&callback);
    VoidResult RemoveSession(const std::string &session_id);

    VoidResult SetOnline(const std::string &user_id);
    VoidResult RefreshOnline(const std::string &user_id);
    VoidResult SetOffline(const std::string &user_id);
    Result<bool> IsOnline(const std::string &user_id);

    VoidResult SaveVerifyCode(const std::string &verify_code_id,
                              const std::string &verify_code);
    Result<std::optional<std::string>>
    GetVerifyCode(const std::string &verify_code_id);
    VoidResult RemoveVerifyCode(const std::string &verify_code_id);

  private:
    drogon::nosql::RedisClientPtr redis_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_COMMON_SESSION_STORE_H_
