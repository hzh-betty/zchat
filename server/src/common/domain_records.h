#ifndef ZCHAT_SERVER_SRC_COMMON_DOMAIN_RECORDS_H_
#define ZCHAT_SERVER_SRC_COMMON_DOMAIN_RECORDS_H_

#include <cstdint>
#include <string>

namespace zchat {

enum class ChatSessionType {
    kSingle = 1,
    kGroup = 2,
};

struct UserRecord {
    std::string user_id;
    std::string nickname;
    std::string description;
    std::string password;
    std::string password_hash_algo;
    std::string phone;
    std::string avatar_id;
};

struct FriendApplyRecord {
    std::string event_id;
    std::string user_id;
    std::string peer_id;
};

struct ChatSessionRecord {
    std::string chat_session_id;
    std::string chat_session_name;
    ChatSessionType chat_session_type = ChatSessionType::kSingle;
};

struct MessageRecord {
    std::string message_id;
    std::string session_id;
    std::string user_id;
    int message_type = 0;
    std::int64_t create_time = 0;
    std::string content;
    std::string file_id;
    std::string file_name;
    std::uint64_t file_size = 0;
};

struct FileRecord {
    std::string file_id;
    std::string file_name;
    std::uint64_t file_size = 0;
    std::string file_content;
    std::string owner_user_id;
    std::string chat_session_id;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_COMMON_DOMAIN_RECORDS_H_
