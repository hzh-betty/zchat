#include "file/file_context.h"

#include "common/runtime.h"

namespace zchat {

FileContext::FileContext(const AppConfig &config)
    : config_(config), db_(MakeDbClient(config.mysql)),
      redis_(MakeRedisClient(config.redis)), file_repository_(db_),
      clients_(config.etcd), sessions_(redis_),
      file_service_(std::make_shared<FileApplicationService>(
          file_repository_, clients_, sessions_)),
      grpc_service_(file_service_) {}

} // namespace zchat
