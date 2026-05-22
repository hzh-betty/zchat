#include "friend/friend_context.h"

#include "common/runtime.h"

namespace zchat {

FriendContext::FriendContext(const AppConfig &config)
    : config_(config), db_(MakeDbClient(config.mysql)),
      redis_(MakeRedisClient(config.redis)), friend_repository_(db_),
      user_repository_(db_), file_repository_(db_), message_repository_(db_),
      sessions_(redis_), notifier_(redis_),
      friend_service_(std::make_shared<FriendApplicationService>(
          friend_repository_, user_repository_, file_repository_,
          message_repository_, sessions_, notifier_)),
      grpc_service_(friend_service_) {}

} // namespace zchat
