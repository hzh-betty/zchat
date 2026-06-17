#ifndef ZCHAT_SERVER_SRC_MESSAGE_MESSAGE_SERVICE_H_
#define ZCHAT_SERVER_SRC_MESSAGE_MESSAGE_SERVICE_H_

#include "common/noncopyable.h"

#include "common/search/message_search_index.h"
#include "common/service_clients.h"
#include "message.pb.h"
#include "message/message_repository.h"

namespace zchat {

class MessageService : public NonCopyable {
  public:
    MessageService(MessageRepository &messages, ServiceClients &clients,
                   MessageSearchIndex &search_index);
    ~MessageService() = default;

    zchat::GetRecentMsgRsp GetRecent(const zchat::GetRecentMsgReq &request);
    zchat::GetMultiRecentMsgRsp
    GetMultiRecent(const zchat::GetMultiRecentMsgReq &request);
    zchat::GetHistoryMsgRsp GetHistory(const zchat::GetHistoryMsgReq &request);
    zchat::MsgSearchRsp Search(const zchat::MsgSearchReq &request);
    VoidResult StoreQueuedMessage(const zchat::MessageInfo &message);

  private:
    template <typename Response, typename Messages>
    Response BuildMessageListResponse(const std::string &request_id,
                                      const Messages &messages);
    VoidResult EnsureCanReadSession(const std::string &request_id,
                                    const std::string &user_id,
                                    const std::string &session_id);

    MessageRepository &messages_;
    ServiceClients &clients_;
    MessageSearchIndex &search_index_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_MESSAGE_MESSAGE_SERVICE_H_