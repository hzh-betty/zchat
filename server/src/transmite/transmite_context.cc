#include "transmite/transmite_context.h"

#include "common/runtime.h"

namespace zchat {

TransmiteContext::TransmiteContext(const AppConfig &config)
    : config_(config), db_(MakeDbClient(config.mysql)),
      redis_(MakeRedisClient(config.redis)), transmite_repository_(db_),
      user_repository_(db_), sessions_(redis_), notifier_(redis_),
      queue_(config.rabbitmq), search_index_(config.elasticsearch),
      transmite_service_(std::make_shared<TransmiteService>(
          transmite_repository_, user_repository_, queue_, search_index_,
          sessions_, notifier_)),
      grpc_service_(transmite_service_) {}

} // namespace zchat
