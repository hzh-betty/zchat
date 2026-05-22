#ifndef ZCHAT_SERVER_SRC_TRANSMITE_TRANSMITE_REPOSITORY_H_
#define ZCHAT_SERVER_SRC_TRANSMITE_TRANSMITE_REPOSITORY_H_

#include "common/noncopyable.h"

#include <memory>
#include <string>
#include <vector>

#include <drogon/orm/DbClient.h>

#include "common/domain_records.h"
#include "common/result.h"
#include "repository/orm_helpers.h"

namespace zchat {

class TransmiteRepository : public NonCopyable {
  public:
    TransmiteRepository() = default;

    virtual ~TransmiteRepository() = default;

    virtual VoidResult InsertMessage(const MessageRecord &message) = 0;
    virtual VoidResult PutFile(const FileRecord &file) = 0;
    virtual Result<std::vector<std::string>>
    ListChatSessionMembers(const std::string &session_id) = 0;
};

class OrmTransmiteRepository final : public TransmiteRepository,
                                     public OrmRepositoryBase {
  public:
    explicit OrmTransmiteRepository(std::shared_ptr<drogon::orm::DbClient> db);

    ~OrmTransmiteRepository() override = default;

    VoidResult InsertMessage(const MessageRecord &message) override;
    VoidResult PutFile(const FileRecord &file) override;
    Result<std::vector<std::string>>
    ListChatSessionMembers(const std::string &session_id) override;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_TRANSMITE_TRANSMITE_REPOSITORY_H_
