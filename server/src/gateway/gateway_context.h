#ifndef ZCHAT_SERVER_SRC_GATEWAY_GATEWAY_CONTEXT_H_
#define ZCHAT_SERVER_SRC_GATEWAY_GATEWAY_CONTEXT_H_

#include "common/noncopyable.h"

#include <memory>

#include <drogon/nosql/RedisClient.h>

#include "common/config.h"
#include "common/session_store.h"
#include "gateway/connection_registry.h"
#include "gateway/grpc_service_clients.h"
#include "gateway/notify_subscriber.h"

namespace zchat {

class GatewayContext : public NonCopyable {
  public:
    explicit GatewayContext(const AppConfig &config);

    ~GatewayContext() = default;

    const AppConfig &config() const { return config_; }
    SessionStore &sessions() { return sessions_; }
    ConnectionRegistry &connections() { return connections_; }
    GrpcServiceClients &grpc_clients() { return grpc_clients_; }

  private:
    AppConfig config_;
    drogon::nosql::RedisClientPtr redis_;
    SessionStore sessions_;
    ConnectionRegistry connections_;
    GrpcServiceClients grpc_clients_;
    NotifySubscriber notify_subscriber_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_GATEWAY_GATEWAY_CONTEXT_H_
