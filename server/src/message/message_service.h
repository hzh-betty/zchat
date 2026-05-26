#ifndef ZCHAT_SERVER_SRC_MESSAGE_MESSAGE_SERVICE_H_
#define ZCHAT_SERVER_SRC_MESSAGE_MESSAGE_SERVICE_H_

#include "common/noncopyable.h"

#include "file/file_repository.h"
#include "friend/friend_repository.h"
#include "message.pb.h"
#include "message/message_repository.h"
#include "message/message_search_index.h"
#include "user/user_repository.h"

namespace zchat {

class MessageService : public NonCopyable {
  public:
    MessageService(MessageRepository &messages, UserRepository &users,
                   FileRepository &files, FriendRepository &friends,
                   MessageSearchIndex &search_index);
    ~MessageService() = default;

    zchat::GetRecentMsgRsp GetRecent(const zchat::GetRecentMsgReq &request);
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
    UserRepository &users_;
    FileRepository &files_;
    FriendRepository &friends_;
    MessageSearchIndex &search_index_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_MESSAGE_MESSAGE_SERVICE_H_
