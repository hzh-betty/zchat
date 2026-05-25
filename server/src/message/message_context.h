#ifndef ZCHAT_SERVER_SRC_MESSAGE_MESSAGE_CONTEXT_H_
#define ZCHAT_SERVER_SRC_MESSAGE_MESSAGE_CONTEXT_H_

#include "common/noncopyable.h"

#include <memory>

#include <drogon/orm/DbClient.h>

#include "common/config.h"
#include "file/file_repository.h"
#include "message/message_grpc_service.h"
#include "message/message_repository.h"
#include "message/message_search_index.h"
#include "message/message_service.h"
#include "transmite/message_queue.h"
#include "user/user_repository.h"

namespace zchat {

class MessageContext : public NonCopyable {
  public:
    explicit MessageContext(const AppConfig &config);
    ~MessageContext() = default;

    MessageGrpcService &grpc_service() { return grpc_service_; }

  private:
    AppConfig config_;
    std::shared_ptr<drogon::orm::DbClient> db_;
    OrmMessageRepository message_repository_;
    OrmUserRepository user_repository_;
    OrmFileRepository file_repository_;
    ConfiguredMessageSearchIndex search_index_;
    std::shared_ptr<MessageService> message_service_;
    ConfiguredMessageQueueConsumer queue_consumer_;
    MessageGrpcService grpc_service_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_MESSAGE_MESSAGE_CONTEXT_H_
