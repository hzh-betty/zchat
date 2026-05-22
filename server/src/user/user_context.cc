#include "user/user_context.h"

#include "common/runtime.h"

namespace zchat {

UserContext::UserContext(const AppConfig &config)
    : config_(config), db_(MakeDbClient(config.mysql)),
      redis_(MakeRedisClient(config.redis)), user_repository_(db_),
      file_repository_(db_), sessions_(redis_), sms_(config.sms.enabled),
      user_service_(std::make_shared<UserApplicationService>(
          user_repository_, file_repository_, sms_, sessions_)),
      grpc_service_(user_service_) {}

} // namespace zchat
