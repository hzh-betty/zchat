#include "gateway/notify_subscriber.h"

#include <utility>

#include "common/logger.h"

namespace zchat {
namespace {

constexpr char kNotifyPrefix[] = "zchat:notify:";

} // namespace

NotifySubscriber::NotifySubscriber(drogon::nosql::RedisClientPtr redis,
                                   ConnectionRegistry &connections)
    : redis_(std::move(redis)), connections_(connections) {}

void NotifySubscriber::Start() {
    subscriber_ = redis_->newSubscriber();
    ZCHAT_LOG_INFO("redis notify subscriber started pattern=zchat:notify:*");
    subscriber_->psubscribe(
        "zchat:notify:*",
        [this](const std::string &channel, const std::string &message) {
            ZCHAT_LOG_DEBUG("redis notify received channel={} size={}B",
                            channel, message.size());
            connections_.SendToUser(UserIdFromChannel(channel), message);
        });
}

std::string NotifySubscriber::UserIdFromChannel(const std::string &channel) {
    const std::string prefix(kNotifyPrefix);
    if (channel.rfind(prefix, 0) != 0) {
        return std::string();
    }
    return channel.substr(prefix.size());
}

} // namespace zchat
