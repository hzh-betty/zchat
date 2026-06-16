#ifndef ZCHAT_SERVER_SRC_COMMON_PROTO_MAPPER_H_
#define ZCHAT_SERVER_SRC_COMMON_PROTO_MAPPER_H_

#include <string>

#include "base.pb.h"
#include "common/domain_records.h"
#include "transmite.pb.h"

namespace zchat {

zchat::UserInfo ToProtoUser(const UserRecord &user,
                            const std::string &avatar_content);
UserRecord FromProtoUser(const zchat::UserInfo &user);
zchat::MessageInfo ToProtoMessage(const MessageRecord &message,
                                  const UserRecord &sender,
                                  const std::string &file_content);
MessageRecord FromProtoMessage(const zchat::MessageInfo &message,
                               std::string *file_content);
MessageRecord ToMessageRecord(const zchat::NewMessageReq &request,
                              const std::string &message_id,
                              const std::string &user_id,
                              std::int64_t create_time,
                              std::string *file_content);

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_COMMON_PROTO_MAPPER_H_
