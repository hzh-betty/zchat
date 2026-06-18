#include "common/notify_publisher.h"

#include <exception>
#include <utility>

#include <drogon/nosql/RedisException.h>
#include <drogon/nosql/RedisResult.h>

#include "common/common_errors.h"
#include "common/logger.h"

namespace zchat {

NotifyPublisher::NotifyPublisher(drogon::nosql::RedisClientPtr redis)
    : redis_(std::move(redis)) {}

drogon::Task<VoidResult>
NotifyPublisher::PublishCoro(const std::string &user_id,
                             const std::string &payload) {
    try {
        co_await redis_->execCommandCoro("publish zchat:notify:%s %b",
                                         user_id.c_str(), payload.data(),
                                         payload.size());
        ZCHAT_LOG_DEBUG("redis notify published user={} size={}B", user_id,
                        payload.size());
        co_return VoidResult::Ok();
    } catch (const drogon::nosql::RedisException &e) {
        ZCHAT_LOG_ERROR("redis notify publish failed user={} error={}", user_id,
                        e.what());
        co_return VoidResult::Fail(e.what());
    } catch (const std::exception &e) {
        ZCHAT_LOG_ERROR("redis notify publish failed user={} error={}", user_id,
                        e.what());
        co_return VoidResult::Fail(e.what());
    }
}

drogon::Task<Result<PublishOutcome>>
NotifyPublisher::PublishBatchCoro(const std::vector<std::string> &user_ids,
                                  const std::string &payload) {
    PublishOutcome outcome;
    if (user_ids.empty()) {
        co_return Result<PublishOutcome>::Ok(std::move(outcome));
    }

    for (const auto &user_id : user_ids) {
        auto result = co_await PublishCoro(user_id, payload);
        if (result.ok()) {
            outcome.succeeded.push_back(user_id);
        } else {
            ZCHAT_LOG_WARN("redis notify publish failed user={} error={}",
                           user_id, result.error().message);
            outcome.failed.push_back(user_id);
        }
    }
    co_return Result<PublishOutcome>::Ok(std::move(outcome));
}

} // namespace zchat
