#include "user/user_context.h"

#include "common/runtime.h"
#include "user/alibaba_sms_client.h"

namespace zchat {

UserContext::UserContext(const AppConfig &config)
    : config_(config), db_(MakeDbClient(config.mysql)),
      redis_(MakeRedisClient(config.redis)), user_repository_(db_),
      file_repository_(db_), search_index_(config.elasticsearch),
      sessions_(redis_), sms_(std::make_unique<AlibabaSmsClient>(config.sms)),
      user_service_(std::make_shared<UserApplicationService>(
          user_repository_, file_repository_, *sms_, sessions_, search_index_)),
      grpc_service_(user_service_) {}

} // namespace zchat
