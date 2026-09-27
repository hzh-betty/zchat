#include "user/user_context.h"

#include <chrono>

#include <drogon/utils/coroutine.h>

#include "common/logger.h"
#include "common/runtime.h"
#include "user/alibaba_sms_client.h"

namespace zchat {

UserContext::UserContext(const AppConfig &config)
    : config_(config), db_(MakeDbClient(config.mysql)),
      redis_(MakeRedisClient(config.redis)), user_repository_(db_),
      clients_(config.etcd), search_index_(config.elasticsearch),
      sessions_(redis_), sms_(std::make_unique<AlibabaSmsClient>(config.sms)),
      user_service_(std::make_shared<UserApplicationService>(
          user_repository_, clients_, *sms_, sessions_, search_index_)),
      grpc_service_(user_service_) {
    index_worker_ = std::thread([this]() {
        std::string cursor;
        while (true) {
            std::chrono::seconds delay(1);
            try {
                const auto result = drogon::sync_wait(
                    user_service_->ReconcileIndexPageCoro(cursor));
                if (!result.ok()) {
                    ZCHAT_LOG_WARN("user index reconciliation failed: {}",
                                   result.error().message);
                    delay = std::chrono::seconds(60);
                } else if (result.value()) {
                    delay = std::chrono::seconds(300);
                }
            } catch (const std::exception &e) {
                ZCHAT_LOG_WARN("user index reconciliation failed: {}",
                               e.what());
                delay = std::chrono::seconds(60);
            }
            std::unique_lock lock(index_mutex_);
            if (index_cv_.wait_for(lock, delay,
                                   [this] { return stop_index_worker_; })) {
                return;
            }
        }
    });
}

UserContext::~UserContext() {
    {
        std::lock_guard lock(index_mutex_);
        stop_index_worker_ = true;
    }
    index_cv_.notify_one();
    if (index_worker_.joinable()) {
        index_worker_.join();
    }
}

} // namespace zchat
