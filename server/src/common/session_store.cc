#include "common/session_store.h"

#include <exception>
#include <utility>

#include <drogon/nosql/RedisException.h>
#include <drogon/nosql/RedisResult.h>

#include "common/common_errors.h"

namespace zchat {
namespace {

template <typename Func> auto RunRedis(Func function) -> decltype(function()) {
    try {
        return function();
    } catch (const drogon::nosql::RedisException &error) {
        using ReturnType = decltype(function());
        return ReturnType::Fail(
            common_errors::RedisOperationFailed().WithDetail(error.what()));
    } catch (const std::exception &error) {
        using ReturnType = decltype(function());
        return ReturnType::Fail(
            common_errors::RedisOperationFailed().WithDetail(error.what()));
    }
}

std::optional<std::string>
OptionalString(const drogon::nosql::RedisResult &result) {
    if (result.isNil()) {
        return std::nullopt;
    }
    return result.asString();
}

} // namespace

SessionStore::SessionStore(drogon::nosql::RedisClientPtr redis)
    : redis_(std::move(redis)) {}

VoidResult SessionStore::SaveSession(const std::string &session_id,
                                     const std::string &user_id) {
    return RunRedis([&]() {
        redis_->execCommandSync<std::string>(
            [](const drogon::nosql::RedisResult &result) {
                return result.getStringForDisplaying();
            },
            "setex zchat:session:%s 604800 %s", session_id.c_str(),
            user_id.c_str());
        return VoidResult::Ok();
    });
}

Result<std::optional<std::string>>
SessionStore::GetUserId(const std::string &session_id) {
    return RunRedis([&]() {
        auto value = redis_->execCommandSync<std::optional<std::string>>(
            [](const drogon::nosql::RedisResult &result) {
                return OptionalString(result);
            },
            "get zchat:session:%s", session_id.c_str());
        return Result<std::optional<std::string>>::Ok(std::move(value));
    });
}

VoidResult SessionStore::RemoveSession(const std::string &session_id) {
    return RunRedis([&]() {
        redis_->execCommandSync<long long>(
            [](const drogon::nosql::RedisResult &result) {
                return result.asInteger();
            },
            "del zchat:session:%s", session_id.c_str());
        return VoidResult::Ok();
    });
}

VoidResult SessionStore::SetOnline(const std::string &user_id) {
    return RunRedis([&]() {
        redis_->execCommandSync<std::string>(
            [](const drogon::nosql::RedisResult &result) {
                return result.getStringForDisplaying();
            },
            "setex zchat:online:%s 300 1", user_id.c_str());
        return VoidResult::Ok();
    });
}

VoidResult SessionStore::RefreshOnline(const std::string &user_id) {
    return RunRedis([&]() {
        redis_->execCommandSync<long long>(
            [](const drogon::nosql::RedisResult &result) {
                return result.asInteger();
            },
            "expire zchat:online:%s 300", user_id.c_str());
        return VoidResult::Ok();
    });
}

VoidResult SessionStore::SetOffline(const std::string &user_id) {
    return RunRedis([&]() {
        redis_->execCommandSync<long long>(
            [](const drogon::nosql::RedisResult &result) {
                return result.asInteger();
            },
            "del zchat:online:%s", user_id.c_str());
        return VoidResult::Ok();
    });
}

Result<bool> SessionStore::IsOnline(const std::string &user_id) {
    return RunRedis([&]() {
        const long long exists = redis_->execCommandSync<long long>(
            [](const drogon::nosql::RedisResult &result) {
                return result.asInteger();
            },
            "exists zchat:online:%s", user_id.c_str());
        return Result<bool>::Ok(exists > 0);
    });
}

VoidResult SessionStore::SaveVerifyCode(const std::string &verify_code_id,
                                        const std::string &verify_code) {
    return RunRedis([&]() {
        redis_->execCommandSync<std::string>(
            [](const drogon::nosql::RedisResult &result) {
                return result.getStringForDisplaying();
            },
            "setex zchat:verify:%s 300 %s", verify_code_id.c_str(),
            verify_code.c_str());
        return VoidResult::Ok();
    });
}

Result<std::optional<std::string>>
SessionStore::GetVerifyCode(const std::string &verify_code_id) {
    return RunRedis([&]() {
        auto value = redis_->execCommandSync<std::optional<std::string>>(
            [](const drogon::nosql::RedisResult &result) {
                return OptionalString(result);
            },
            "get zchat:verify:%s", verify_code_id.c_str());
        return Result<std::optional<std::string>>::Ok(std::move(value));
    });
}

VoidResult SessionStore::RemoveVerifyCode(const std::string &verify_code_id) {
    return RunRedis([&]() {
        redis_->execCommandSync<long long>(
            [](const drogon::nosql::RedisResult &result) {
                return result.asInteger();
            },
            "del zchat:verify:%s", verify_code_id.c_str());
        return VoidResult::Ok();
    });
}

} // namespace zchat
