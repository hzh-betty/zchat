#ifndef ZCHAT_SERVER_SRC_FILE_FILE_REPOSITORY_H_
#define ZCHAT_SERVER_SRC_FILE_FILE_REPOSITORY_H_

#include "common/noncopyable.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <drogon/orm/DbClient.h>
#include <drogon/utils/coroutine.h>

#include "common/domain_records.h"
#include "common/orm_helpers.h"
#include "common/result.h"

namespace zchat {

class FileRepository : public NonCopyable {
  public:
    FileRepository() = default;
    virtual ~FileRepository() = default;

    virtual drogon::Task<VoidResult> PutFileCoro(const FileRecord &file) = 0;
    virtual drogon::Task<Result<std::optional<FileRecord>>>
    GetFileCoro(const std::string &file_id) = 0;
    virtual drogon::Task<Result<std::vector<FileRecord>>>
    FindFilesByIdsCoro(const std::vector<std::string> &file_ids) = 0;
};

class OrmFileRepository final : public FileRepository,
                                public OrmRepositoryBase {
  public:
    explicit OrmFileRepository(std::shared_ptr<drogon::orm::DbClient> db);
    ~OrmFileRepository() override = default;

    drogon::Task<VoidResult> PutFileCoro(const FileRecord &file) override;
    drogon::Task<Result<std::optional<FileRecord>>>
    GetFileCoro(const std::string &file_id) override;
    drogon::Task<Result<std::vector<FileRecord>>>
    FindFilesByIdsCoro(const std::vector<std::string> &file_ids) override;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_FILE_FILE_REPOSITORY_H_
