#include "transmite/transmite_context.h"

#include "common/runtime.h"

namespace zchat {

TransmiteContext::TransmiteContext(const AppConfig &config)
    : config_(config), redis_(MakeRedisClient(config.redis)), sessions_(redis_),
      notifier_(redis_), queue_(config.rabbitmq), clients_(config.etcd),
      transmite_service_(std::make_shared<TransmiteService>(
          queue_, sessions_, notifier_, clients_)),
      grpc_service_(transmite_service_) {}

} // namespace zchat