#ifndef ZCHAT_SERVER_SRC_FILE_FILE_CONTEXT_H_
#define ZCHAT_SERVER_SRC_FILE_FILE_CONTEXT_H_

#include "common/noncopyable.h"

#include <memory>

#include <drogon/orm/DbClient.h>

#include "common/config.h"
#include "file/file_grpc_service.h"
#include "file/file_repository.h"
#include "file/file_service.h"

namespace zchat {

class FileContext : public NonCopyable {
  public:
    explicit FileContext(const AppConfig &config);

    ~FileContext() = default;

    FileGrpcService &grpc_service() { return grpc_service_; }

  private:
    AppConfig config_;
    std::shared_ptr<drogon::orm::DbClient> db_;
    OrmFileRepository file_repository_;
    std::shared_ptr<FileApplicationService> file_service_;
    FileGrpcService grpc_service_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_FILE_FILE_CONTEXT_H_
