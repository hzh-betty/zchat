#ifndef ZCHAT_SERVER_SRC_FILE_FILE_REPOSITORY_H_
#define ZCHAT_SERVER_SRC_FILE_FILE_REPOSITORY_H_

#include "common/noncopyable.h"

#include <memory>
#include <optional>
#include <string>

#include <drogon/orm/DbClient.h>

#include "common/domain_records.h"
#include "common/result.h"
#include "repository/orm_helpers.h"

namespace zchat {

class FileRepository : public NonCopyable {
  public:
    FileRepository() = default;

    virtual ~FileRepository() = default;

    virtual VoidResult PutFile(const FileRecord &file) = 0;
    virtual Result<std::optional<FileRecord>>
    GetFile(const std::string &file_id) = 0;
};

class OrmFileRepository final : public FileRepository,
                                public OrmRepositoryBase {
  public:
    explicit OrmFileRepository(std::shared_ptr<drogon::orm::DbClient> db);

    ~OrmFileRepository() override = default;

    VoidResult PutFile(const FileRecord &file) override;
    Result<std::optional<FileRecord>>
    GetFile(const std::string &file_id) override;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_FILE_FILE_REPOSITORY_H_
