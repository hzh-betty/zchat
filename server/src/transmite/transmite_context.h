#ifndef ZCHAT_SERVER_SRC_TRANSMITE_TRANSMITE_CONTEXT_H_
#define ZCHAT_SERVER_SRC_TRANSMITE_TRANSMITE_CONTEXT_H_

#include "common/noncopyable.h"

#include <memory>

#include <drogon/nosql/RedisClient.h>

#include "common/config.h"
#include "common/notify_publisher.h"
#include "common/service_clients.h"
#include "common/session_store.h"
#include "transmite/message_queue.h"
#include "transmite/transmite_grpc_service.h"
#include "transmite/transmite_service.h"

namespace zchat {

class TransmiteContext : public NonCopyable {
  public:
    explicit TransmiteContext(const AppConfig &config);
    ~TransmiteContext() = default;

    TransmiteGrpcService &grpc_service() { return grpc_service_; }

  private:
    AppConfig config_;
    drogon::nosql::RedisClientPtr redis_;
    SessionStore sessions_;
    NotifyPublisher notifier_;
    ConfiguredMessageQueuePublisher queue_;
    ServiceClients clients_;
    std::shared_ptr<TransmiteService> transmite_service_;
    TransmiteGrpcService grpc_service_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_TRANSMITE_TRANSMITE_CONTEXT_H_