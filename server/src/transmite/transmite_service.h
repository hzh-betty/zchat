#ifndef ZCHAT_SERVER_SRC_TRANSMITE_TRANSMITE_SERVICE_H_
#define ZCHAT_SERVER_SRC_TRANSMITE_TRANSMITE_SERVICE_H_

#include "common/noncopyable.h"

#include "common/notify_publisher.h"
#include "common/session_store.h"
#include "message/message_search_index.h"
#include "transmite.pb.h"
#include "transmite/message_queue.h"
#include "transmite/transmite_repository.h"
#include "user/user_repository.h"

namespace zchat {

class TransmiteService : public NonCopyable {
  public:
    TransmiteService(TransmiteRepository &repository, UserRepository &users,
                     MessageQueuePublisher &queue,
                     MessageSearchIndex &search_index, SessionStore &sessions,
                     NotifyPublisher &notifier);
    ~TransmiteService() = default;

    zchat::NewMessageRsp NewMessage(const zchat::NewMessageReq &request);

  private:
    TransmiteRepository &repository_;
    UserRepository &users_;
    MessageQueuePublisher &queue_;
    MessageSearchIndex &search_index_;
    SessionStore &sessions_;
    NotifyPublisher &notifier_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_TRANSMITE_TRANSMITE_SERVICE_H_
