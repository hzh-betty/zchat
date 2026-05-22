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
#include "common/result.h"
#include "repository/orm_helpers.h"

namespace zchat {

class MessageRepository : public NonCopyable {
  public:
    MessageRepository() = default;

    virtual ~MessageRepository() = default;

    virtual VoidResult InsertMessage(const MessageRecord &message) = 0;
    virtual Result<std::vector<MessageRecord>>
    ListRecentMessages(const std::string &session_id, int count) = 0;
    virtual Result<std::vector<MessageRecord>>
    ListMessagesByTime(const std::string &session_id, std::int64_t start_time,
                       std::int64_t end_time) = 0;
    virtual Result<std::vector<MessageRecord>>
    SearchMessages(const std::string &session_id,
                   const std::string &keyword) = 0;
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
    Result<std::vector<MessageRecord>>
    ListMessagesByTime(const std::string &session_id, std::int64_t start_time,
                       std::int64_t end_time) override;
    Result<std::vector<MessageRecord>>
    SearchMessages(const std::string &session_id,
                   const std::string &keyword) override;
    Result<std::optional<MessageRecord>>
    LastMessage(const std::string &session_id) override;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_MESSAGE_MESSAGE_REPOSITORY_H_
