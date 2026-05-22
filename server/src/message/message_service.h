#ifndef ZCHAT_SERVER_SRC_MESSAGE_MESSAGE_SERVICE_H_
#define ZCHAT_SERVER_SRC_MESSAGE_MESSAGE_SERVICE_H_

#include "common/noncopyable.h"

#include "file/file_repository.h"
#include "message.pb.h"
#include "message/message_repository.h"
#include "message/message_search_index.h"
#include "user/user_repository.h"

namespace zchat {

class MessageService : public NonCopyable {
  public:
    MessageService(MessageRepository &messages, UserRepository &users,
                   FileRepository &files, MessageSearchIndex &search_index);
    ~MessageService() = default;

    zchat::GetRecentMsgRsp GetRecent(const zchat::GetRecentMsgReq &request);
    zchat::GetHistoryMsgRsp GetHistory(const zchat::GetHistoryMsgReq &request);
    zchat::MsgSearchRsp Search(const zchat::MsgSearchReq &request);

  private:
    template <typename Response, typename Messages>
    Response BuildMessageListResponse(const std::string &request_id,
                                      const Messages &messages);

    MessageRepository &messages_;
    UserRepository &users_;
    FileRepository &files_;
    MessageSearchIndex &search_index_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_MESSAGE_MESSAGE_SERVICE_H_
