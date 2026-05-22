#include "common/notify_publisher.h"

#include <exception>
#include <utility>

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

} // namespace zchat
