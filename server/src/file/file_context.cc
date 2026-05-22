#include "file/file_context.h"

#include "common/runtime.h"

namespace zchat {

FileContext::FileContext(const AppConfig &config)
    : config_(config), db_(MakeDbClient(config.mysql)), file_repository_(db_),
      file_service_(std::make_shared<FileApplicationService>(file_repository_)),
      grpc_service_(file_service_) {}

} // namespace zchat
