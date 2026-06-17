#ifndef ZCHAT_SERVER_SRC_FRIEND_FRIEND_CONTEXT_H_
#define ZCHAT_SERVER_SRC_FRIEND_FRIEND_CONTEXT_H_

#include "common/noncopyable.h"

#include <memory>

#include <drogon/nosql/RedisClient.h>
#include <drogon/orm/DbClient.h>

#include "common/config.h"
#include "common/notify_publisher.h"
#include "common/service_clients.h"
#include "common/session_store.h"
#include "friend/friend_grpc_service.h"
#include "friend/friend_repository.h"
#include "friend/friend_service.h"

namespace zchat {

class FriendContext : public NonCopyable {
  public:
    explicit FriendContext(const AppConfig &config);

    ~FriendContext() = default;

    FriendGrpcService &grpc_service() { return grpc_service_; }

  private:
    AppConfig config_;
    std::shared_ptr<drogon::orm::DbClient> db_;
    drogon::nosql::RedisClientPtr redis_;
    OrmFriendRepository friend_repository_;
    SessionStore sessions_;
    NotifyPublisher notifier_;
    ServiceClients clients_;
    std::shared_ptr<FriendApplicationService> friend_service_;
    FriendGrpcService grpc_service_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_FRIEND_FRIEND_CONTEXT_H_