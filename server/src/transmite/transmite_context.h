#ifndef ZCHAT_SERVER_SRC_TRANSMITE_TRANSMITE_CONTEXT_H_
#define ZCHAT_SERVER_SRC_TRANSMITE_TRANSMITE_CONTEXT_H_

#include "common/noncopyable.h"

#include <memory>

#include <drogon/nosql/RedisClient.h>
#include <drogon/orm/DbClient.h>

#include "common/config.h"
#include "common/notify_publisher.h"
#include "common/session_store.h"
#include "message/message_search_index.h"
#include "transmite/message_queue.h"
#include "transmite/transmite_grpc_service.h"
#include "transmite/transmite_repository.h"
#include "transmite/transmite_service.h"
#include "user/user_repository.h"

namespace zchat {

class TransmiteContext : public NonCopyable {
  public:
    explicit TransmiteContext(const AppConfig &config);
    ~TransmiteContext() = default;

    TransmiteGrpcService &grpc_service() { return grpc_service_; }

  private:
    AppConfig config_;
    std::shared_ptr<drogon::orm::DbClient> db_;
    drogon::nosql::RedisClientPtr redis_;
    OrmTransmiteRepository transmite_repository_;
    OrmUserRepository user_repository_;
    SessionStore sessions_;
    NotifyPublisher notifier_;
    ConfiguredMessageQueuePublisher queue_;
    ConfiguredMessageSearchIndex search_index_;
    std::shared_ptr<TransmiteService> transmite_service_;
    TransmiteGrpcService grpc_service_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_TRANSMITE_TRANSMITE_CONTEXT_H_
