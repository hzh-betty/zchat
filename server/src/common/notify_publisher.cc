#include "common/notify_publisher.h"

#include <atomic>
#include <chrono>
#include <exception>
#include <future>
#include <mutex>
#include <utility>
#include <vector>

#include <drogon/nosql/RedisException.h>
#include <drogon/nosql/RedisResult.h>

#include "common/logger.h"

namespace zchat {

NotifyPublisher::NotifyPublisher(drogon::nosql::RedisClientPtr redis)
    : redis_(std::move(redis)) {}

VoidResult NotifyPublisher::Publish(const std::string &user_id,
                                    const std::string &payload) {
    try {
        redis_->execCommandSync<long long>(
            [](const drogon::nosql::RedisResult &result) {
                return result.asInteger();
            },
            "publish zchat:notify:%s %b", user_id.c_str(), payload.data(),
            payload.size());
        ZCHAT_LOG_DEBUG("redis notify published user={} size={}B", user_id,
                        payload.size());
        return VoidResult::Ok();
    } catch (const drogon::nosql::RedisException &error) {
        ZCHAT_LOG_ERROR("redis notify publish failed user={} error={}", user_id,
                        error.what());
        return VoidResult::Fail(error.what());
    } catch (const std::exception &error) {
        ZCHAT_LOG_ERROR("redis notify publish failed user={} error={}", user_id,
                        error.what());
        return VoidResult::Fail(error.what());
    }
}

namespace {

struct BatchState {
    std::mutex mutex;
    std::size_t remaining = 0;
    PublishOutcome outcome;
    std::promise<Result<PublishOutcome>> promise;
    void Finish(Result<PublishOutcome> result) {
        try {
            promise.set_value(std::move(result));
        } catch (...) {
        }
    }
};

} // namespace

Result<PublishOutcome>
NotifyPublisher::PublishBatch(const std::vector<std::string> &user_ids,
                              const std::string &payload) {
    if (user_ids.empty()) {
        return Result<PublishOutcome>::Ok(PublishOutcome{});
    }

    auto state = std::make_shared<BatchState>();
    state->remaining = user_ids.size();

    for (const auto &user_id : user_ids) {
        redis_->execCommandAsync(
            [state, user_id](const drogon::nosql::RedisResult &) {
                std::lock_guard<std::mutex> lock(state->mutex);
                state->outcome.succeeded.push_back(user_id);
                if (--state->remaining == 0) {
                    state->Finish(Result<PublishOutcome>::Ok(
                        std::move(state->outcome)));
                }
            },
            [state, user_id](const drogon::nosql::RedisException &err) {
                ZCHAT_LOG_WARN("redis notify publish failed user={} error={}",
                               user_id, err.what());
                std::lock_guard<std::mutex> lock(state->mutex);
                state->outcome.failed.push_back(user_id);
                if (--state->remaining == 0) {
                    state->Finish(Result<PublishOutcome>::Ok(
                        std::move(state->outcome)));
                }
            },
            "publish zchat:notify:%s %b", user_id.c_str(), payload.data(),
            payload.size());
    }

    auto future = state->promise.get_future();
    if (future.wait_for(std::chrono::seconds(3)) !=
        std::future_status::ready) {
        ZCHAT_LOG_ERROR("redis notify publish batch timeout total={} failed={}",
                        user_ids.size(), state->outcome.failed.size());
        return Result<PublishOutcome>::Fail(AppError::WithCode(
            ErrorCode::kExternalServiceError,
            "redis publish batch timeout"));
    }
    return future.get();
}

} // namespace zchat
