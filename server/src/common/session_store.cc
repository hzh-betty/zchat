#include "common/session_store.h"

#include <exception>
#include <utility>

#include <drogon/nosql/RedisException.h>
#include <drogon/nosql/RedisResult.h>

#include "common/common_errors.h"

namespace zchat {

namespace {

constexpr int kMaxLoginFailCount = 5;
constexpr int kLockSeconds = 900;
constexpr int kFailWindowSeconds = 900;

} // namespace

SessionStore::SessionStore(drogon::nosql::RedisClientPtr redis)
    : redis_(std::move(redis)) {}

drogon::Task<Result<std::optional<std::string>>>
SessionStore::GetUserIdCoro(const std::string &session_id) {
    try {
        auto result = co_await redis_->execCommandCoro("get zchat:session:%s",
                                                       session_id.c_str());
        if (result.isNil()) {
            co_return Result<std::optional<std::string>>::Ok(std::nullopt);
        }
        co_return Result<std::optional<std::string>>::Ok(
            std::make_optional(result.asString()));
    } catch (const drogon::nosql::RedisException &e) {
        co_return Result<std::optional<std::string>>::Fail(
            common_errors::RedisOperationFailed().WithDetail(e.what()));
    } catch (const std::exception &e) {
        co_return Result<std::optional<std::string>>::Fail(
            common_errors::RedisOperationFailed().WithDetail(e.what()));
    }
}

drogon::Task<VoidResult>
SessionStore::SaveSessionCoro(const std::string &session_id,
                              const std::string &user_id) {
    try {
        co_await redis_->execCommandCoro("setex zchat:session:%s 604800 %s",
                                         session_id.c_str(), user_id.c_str());
        co_return VoidResult::Ok();
    } catch (const drogon::nosql::RedisException &e) {
        co_return VoidResult::Fail(
            common_errors::RedisOperationFailed().WithDetail(e.what()));
    } catch (const std::exception &e) {
        co_return VoidResult::Fail(
            common_errors::RedisOperationFailed().WithDetail(e.what()));
    }
}

drogon::Task<VoidResult>
SessionStore::RemoveSessionCoro(const std::string &session_id) {
    try {
        co_await redis_->execCommandCoro("del zchat:session:%s",
                                         session_id.c_str());
        co_return VoidResult::Ok();
    } catch (const drogon::nosql::RedisException &e) {
        co_return VoidResult::Fail(
            common_errors::RedisOperationFailed().WithDetail(e.what()));
    } catch (const std::exception &e) {
        co_return VoidResult::Fail(
            common_errors::RedisOperationFailed().WithDetail(e.what()));
    }
}

drogon::Task<VoidResult>
SessionStore::SetOnlineCoro(const std::string &user_id) {
    try {
        co_await redis_->execCommandCoro("setex zchat:online:%s 300 1",
                                         user_id.c_str());
        co_return VoidResult::Ok();
    } catch (const drogon::nosql::RedisException &e) {
        co_return VoidResult::Fail(
            common_errors::RedisOperationFailed().WithDetail(e.what()));
    } catch (const std::exception &e) {
        co_return VoidResult::Fail(
            common_errors::RedisOperationFailed().WithDetail(e.what()));
    }
}

drogon::Task<Result<bool>>
SessionStore::SetOnlineIfAbsentCoro(const std::string &user_id) {
    try {
        auto result = co_await redis_->execCommandCoro(
            "set zchat:online:%s 1 NX EX 300", user_id.c_str());
        co_return Result<bool>::Ok(result.getStringForDisplaying() == "OK");
    } catch (const drogon::nosql::RedisException &e) {
        co_return Result<bool>::Fail(
            common_errors::RedisOperationFailed().WithDetail(e.what()));
    } catch (const std::exception &e) {
        co_return Result<bool>::Fail(
            common_errors::RedisOperationFailed().WithDetail(e.what()));
    }
}

drogon::Task<VoidResult>
SessionStore::RefreshOnlineCoro(const std::string &user_id) {
    try {
        co_await redis_->execCommandCoro("expire zchat:online:%s 300",
                                         user_id.c_str());
        co_return VoidResult::Ok();
    } catch (const drogon::nosql::RedisException &e) {
        co_return VoidResult::Fail(
            common_errors::RedisOperationFailed().WithDetail(e.what()));
    } catch (const std::exception &e) {
        co_return VoidResult::Fail(
            common_errors::RedisOperationFailed().WithDetail(e.what()));
    }
}

drogon::Task<VoidResult>
SessionStore::SetOfflineCoro(const std::string &user_id) {
    try {
        co_await redis_->execCommandCoro("del zchat:online:%s",
                                         user_id.c_str());
        co_return VoidResult::Ok();
    } catch (const drogon::nosql::RedisException &e) {
        co_return VoidResult::Fail(
            common_errors::RedisOperationFailed().WithDetail(e.what()));
    } catch (const std::exception &e) {
        co_return VoidResult::Fail(
            common_errors::RedisOperationFailed().WithDetail(e.what()));
    }
}

drogon::Task<Result<bool>>
SessionStore::IsOnlineCoro(const std::string &user_id) {
    try {
        auto result = co_await redis_->execCommandCoro("exists zchat:online:%s",
                                                       user_id.c_str());
        co_return Result<bool>::Ok(result.asInteger() > 0);
    } catch (const drogon::nosql::RedisException &e) {
        co_return Result<bool>::Fail(
            common_errors::RedisOperationFailed().WithDetail(e.what()));
    } catch (const std::exception &e) {
        co_return Result<bool>::Fail(
            common_errors::RedisOperationFailed().WithDetail(e.what()));
    }
}

drogon::Task<VoidResult>
SessionStore::SaveVerifyCodeCoro(const std::string &verify_code_id,
                                 const std::string &verify_code) {
    try {
        co_await redis_->execCommandCoro("setex zchat:verify:%s 300 %s",
                                         verify_code_id.c_str(),
                                         verify_code.c_str());
        co_return VoidResult::Ok();
    } catch (const drogon::nosql::RedisException &e) {
        co_return VoidResult::Fail(
            common_errors::RedisOperationFailed().WithDetail(e.what()));
    } catch (const std::exception &e) {
        co_return VoidResult::Fail(
            common_errors::RedisOperationFailed().WithDetail(e.what()));
    }
}

drogon::Task<Result<std::optional<std::string>>>
SessionStore::GetVerifyCodeCoro(const std::string &verify_code_id) {
    try {
        auto result = co_await redis_->execCommandCoro("get zchat:verify:%s",
                                                       verify_code_id.c_str());
        if (result.isNil()) {
            co_return Result<std::optional<std::string>>::Ok(std::nullopt);
        }
        co_return Result<std::optional<std::string>>::Ok(
            std::make_optional(result.asString()));
    } catch (const drogon::nosql::RedisException &e) {
        co_return Result<std::optional<std::string>>::Fail(
            common_errors::RedisOperationFailed().WithDetail(e.what()));
    } catch (const std::exception &e) {
        co_return Result<std::optional<std::string>>::Fail(
            common_errors::RedisOperationFailed().WithDetail(e.what()));
    }
}

drogon::Task<VoidResult>
SessionStore::RemoveVerifyCodeCoro(const std::string &verify_code_id) {
    try {
        co_await redis_->execCommandCoro("del zchat:verify:%s",
                                         verify_code_id.c_str());
        co_return VoidResult::Ok();
    } catch (const drogon::nosql::RedisException &e) {
        co_return VoidResult::Fail(
            common_errors::RedisOperationFailed().WithDetail(e.what()));
    } catch (const std::exception &e) {
        co_return VoidResult::Fail(
            common_errors::RedisOperationFailed().WithDetail(e.what()));
    }
}

drogon::Task<Result<int>>
SessionStore::RecordLoginFailCoro(const std::string &user_id) {
    try {
        auto count_result = co_await redis_->execCommandCoro(
            "incr zchat:loginfail:%s", user_id.c_str());
        long long count = count_result.asInteger();
        if (count == 1) {
            co_await redis_->execCommandCoro("expire zchat:loginfail:%s %d",
                                             user_id.c_str(),
                                             kFailWindowSeconds);
        }
        if (count >= kMaxLoginFailCount) {
            co_await redis_->execCommandCoro("setex zchat:loginlock:%s %d 1",
                                             user_id.c_str(), kLockSeconds);
        }
        co_return Result<int>::Ok(static_cast<int>(count));
    } catch (const drogon::nosql::RedisException &e) {
        co_return Result<int>::Fail(
            common_errors::RedisOperationFailed().WithDetail(e.what()));
    } catch (const std::exception &e) {
        co_return Result<int>::Fail(
            common_errors::RedisOperationFailed().WithDetail(e.what()));
    }
}

drogon::Task<Result<bool>>
SessionStore::IsAccountLockedCoro(const std::string &user_id) {
    try {
        auto result = co_await redis_->execCommandCoro(
            "exists zchat:loginlock:%s", user_id.c_str());
        co_return Result<bool>::Ok(result.asInteger() > 0);
    } catch (const drogon::nosql::RedisException &e) {
        co_return Result<bool>::Fail(
            common_errors::RedisOperationFailed().WithDetail(e.what()));
    } catch (const std::exception &e) {
        co_return Result<bool>::Fail(
            common_errors::RedisOperationFailed().WithDetail(e.what()));
    }
}

drogon::Task<VoidResult>
SessionStore::ClearLoginFailCoro(const std::string &user_id) {
    try {
        co_await redis_->execCommandCoro(
            "del zchat:loginfail:%s zchat:loginlock:%s", user_id.c_str(),
            user_id.c_str());
        co_return VoidResult::Ok();
    } catch (const drogon::nosql::RedisException &e) {
        co_return VoidResult::Fail(
            common_errors::RedisOperationFailed().WithDetail(e.what()));
    } catch (const std::exception &e) {
        co_return VoidResult::Fail(
            common_errors::RedisOperationFailed().WithDetail(e.what()));
    }
}

} // namespace zchat
