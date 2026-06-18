#ifndef ZCHAT_SERVER_SRC_MESSAGE_MESSAGE_SERVICE_H_
#define ZCHAT_SERVER_SRC_MESSAGE_MESSAGE_SERVICE_H_

#include "common/noncopyable.h"

#include <drogon/utils/coroutine.h>

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

    drogon::Task<zchat::GetRecentMsgRsp>
    GetRecentCoro(const zchat::GetRecentMsgReq &request);
    drogon::Task<zchat::GetMultiRecentMsgRsp>
    GetMultiRecentCoro(const zchat::GetMultiRecentMsgReq &request);
    drogon::Task<zchat::GetHistoryMsgRsp>
    GetHistoryCoro(const zchat::GetHistoryMsgReq &request);
    drogon::Task<zchat::MsgSearchRsp>
    SearchCoro(const zchat::MsgSearchReq &request);
    drogon::Task<VoidResult>
    StoreQueuedMessageCoro(const zchat::MessageInfo &message);

  private:
    template <typename Response, typename Messages>
    drogon::Task<Response>
    BuildMessageListResponseCoro(const std::string &request_id,
                                 const Messages &messages);
    drogon::Task<VoidResult>
    EnsureCanReadSessionCoro(const std::string &user_id,
                             const std::string &session_id);

    MessageRepository &messages_;
    ServiceClients &clients_;
    MessageSearchIndex &search_index_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_MESSAGE_MESSAGE_SERVICE_H_
