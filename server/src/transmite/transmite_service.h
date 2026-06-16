#ifndef ZCHAT_SERVER_SRC_TRANSMITE_TRANSMITE_SERVICE_H_
#define ZCHAT_SERVER_SRC_TRANSMITE_TRANSMITE_SERVICE_H_

#include "common/noncopyable.h"

#include "common/notify_publisher.h"
#include "common/service_clients.h"
#include "common/session_store.h"
#include "transmite.pb.h"
#include "transmite/message_queue.h"

namespace zchat {

class TransmiteService : public NonCopyable {
  public:
    TransmiteService(MessageQueuePublisher &queue, SessionStore &sessions,
                     NotifyPublisher &notifier, ServiceClients &clients);
    ~TransmiteService() = default;

    zchat::NewMessageRsp NewMessage(const zchat::NewMessageReq &request);

  private:
    MessageQueuePublisher &queue_;
    SessionStore &sessions_;
    NotifyPublisher &notifier_;
    ServiceClients &clients_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_TRANSMITE_TRANSMITE_SERVICE_H_