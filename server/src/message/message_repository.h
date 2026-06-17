#ifndef ZCHAT_SERVER_SRC_MESSAGE_MESSAGE_REPOSITORY_H_
#define ZCHAT_SERVER_SRC_MESSAGE_MESSAGE_REPOSITORY_H_

#include "common/noncopyable.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <drogon/orm/DbClient.h>

#include "common/domain_records.h"
#include "common/orm_helpers.h"
#include "common/result.h"

namespace zchat {

class MessageRepository : public NonCopyable {
  public:
    MessageRepository() = default;

    virtual ~MessageRepository() = default;

    virtual VoidResult InsertMessage(const MessageRecord &message) = 0;
    virtual Result<std::vector<MessageRecord>>
    ListRecentMessages(const std::string &session_id, int count) = 0;
    virtual Result<std::vector<MessageRecord>> ListLastMessagesForSessions(
        const std::vector<std::string> &session_ids) = 0;
    virtual Result<std::vector<MessageRecord>>
    ListMessagesByTime(const std::string &session_id, std::int64_t start_time,
                       std::int64_t end_time, int max_count,
                       std::optional<std::string> before_msg_id) = 0;
    virtual Result<std::optional<MessageRecord>>
    LastMessage(const std::string &session_id) = 0;
};

class OrmMessageRepository final : public MessageRepository,
                                   public OrmRepositoryBase {
  public:
    explicit OrmMessageRepository(std::shared_ptr<drogon::orm::DbClient> db);

    ~OrmMessageRepository() override = default;

    VoidResult InsertMessage(const MessageRecord &message) override;
    Result<std::vector<MessageRecord>>
    ListRecentMessages(const std::string &session_id, int count) override;
    Result<std::vector<MessageRecord>> ListLastMessagesForSessions(
        const std::vector<std::string> &session_ids) override;
    Result<std::vector<MessageRecord>>
    ListMessagesByTime(const std::string &session_id, std::int64_t start_time,
                       std::int64_t end_time, int max_count,
                       std::optional<std::string> before_msg_id) override;
    Result<std::optional<MessageRecord>>
    LastMessage(const std::string &session_id) override;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_MESSAGE_MESSAGE_REPOSITORY_H_
