#ifndef ZCHAT_SERVER_SRC_GATEWAY_NOTIFY_SUBSCRIBER_H_
#define ZCHAT_SERVER_SRC_GATEWAY_NOTIFY_SUBSCRIBER_H_

#include "common/noncopyable.h"

#include <memory>
#include <string>

#include <drogon/nosql/RedisClient.h>
#include <drogon/nosql/RedisSubscriber.h>

#include "gateway/connection_registry.h"

namespace zchat {

class NotifySubscriber : public NonCopyable {
  public:
    NotifySubscriber(drogon::nosql::RedisClientPtr redis,
                     ConnectionRegistry &connections);

    ~NotifySubscriber() = default;

    void Start();

  private:
    static std::string UserIdFromChannel(const std::string &channel);

    drogon::nosql::RedisClientPtr redis_;
    std::shared_ptr<drogon::nosql::RedisSubscriber> subscriber_;
    ConnectionRegistry &connections_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_GATEWAY_NOTIFY_SUBSCRIBER_H_
