#include "transmite/message_queue.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <amqpcpp.h>
#include <amqpcpp/libevent.h>
#include <event2/event.h>
#include <event2/thread.h>
#include <openssl/ssl.h>

#include "common/logger.h"

namespace zchat {
namespace {

struct EventBaseDeleter {
    void operator()(event_base *base) const {
        if (base != nullptr) {
            event_base_free(base);
        }
    }
};

event_base *CreateEventBase() {
    static const int threads_ready = evthread_use_pthreads();
    if (threads_ready != 0) {
        return nullptr;
    }
    return event_base_new();
}

class RuntimeHandler final : public AMQP::LibEventHandler {
  public:
    explicit RuntimeHandler(event_base *base,
                            const RabbitmqConfig *config = nullptr)
        : AMQP::LibEventHandler(base), tls_config_(config) {}

    void onReady(AMQP::TcpConnection *) override {
        ready_.store(true);
        std::lock_guard<std::mutex> lock(error_mutex_);
        error_.clear();
    }

    void onError(AMQP::TcpConnection *, const char *message) override {
        ready_.store(false);
        std::lock_guard<std::mutex> lock(error_mutex_);
        error_ = message == nullptr ? "unknown RabbitMQ error" : message;
    }

    void onClosed(AMQP::TcpConnection *) override { ready_.store(false); }

    bool onSecuring(AMQP::TcpConnection *, SSL *ssl) override {
        if (tls_config_ != nullptr && tls_config_->tls.enable &&
            !tls_config_->tls.ca_path.empty()) {
            SSL_CTX *ctx = SSL_get_SSL_CTX(ssl);
            if (ctx != nullptr) {
                SSL_CTX_load_verify_locations(
                    ctx, tls_config_->tls.ca_path.c_str(), nullptr);
                if (!tls_config_->tls.cert_path.empty() &&
                    !tls_config_->tls.key_path.empty()) {
                    SSL_CTX_use_certificate_file(
                        ctx, tls_config_->tls.cert_path.c_str(),
                        SSL_FILETYPE_PEM);
                    SSL_CTX_use_PrivateKey_file(
                        ctx, tls_config_->tls.key_path.c_str(),
                        SSL_FILETYPE_PEM);
                }
            }
        }
        return true;
    }

    bool ready() const { return ready_.load(); }
    std::string error() const {
        std::lock_guard<std::mutex> lock(error_mutex_);
        return error_;
    }

  private:
    const RabbitmqConfig *tls_config_;
    std::atomic_bool ready_{false};
    mutable std::mutex error_mutex_;
    std::string error_;
};

} // namespace

class AmqpPublisherRuntime {
  public:
    explicit AmqpPublisherRuntime(const RabbitmqConfig &config)
        : base_(CreateEventBase()), handler_(base_.get(), &config),
          address_(BuildRabbitmqAddress(config)),
          connection_(&handler_, address_), channel_(&connection_),
          exchange_(config.exchange), queue_(config.queue),
          routing_key_(config.routing_key) {
        if (!base_) {
            return;
        }
        channel_.onError([this](const char *message) {
            std::lock_guard<std::mutex> lock(error_mutex_);
            error_ =
                message == nullptr ? "unknown RabbitMQ channel error" : message;
        });
        channel_.declareExchange(exchange_, AMQP::direct, AMQP::durable);
        channel_.declareQueue(queue_, AMQP::durable);
        channel_.bindQueue(exchange_, queue_, routing_key_);
        thread_ = std::thread([this]() { event_base_dispatch(base_.get()); });
    }

    AmqpPublisherRuntime(const AmqpPublisherRuntime &) = delete;
    AmqpPublisherRuntime &operator=(const AmqpPublisherRuntime &) = delete;
    AmqpPublisherRuntime(AmqpPublisherRuntime &&) = delete;
    AmqpPublisherRuntime &operator=(AmqpPublisherRuntime &&) = delete;

    ~AmqpPublisherRuntime() {
        if (!base_) {
            return;
        }
        connection_.close();
        event_base_loopbreak(base_.get());
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    VoidResult Publish(const std::string &payload) {
        if (!base_) {
            return VoidResult::Fail(AppError::WithCode(
                ErrorCode::kExternalServiceError,
                "rabbitmq event loop initialization failed"));
        }
        if (!handler_.ready()) {
            const std::string handler_error = handler_.error();
            const std::string error = handler_error.empty()
                                          ? "rabbitmq connection is not ready"
                                          : handler_error;
            return VoidResult::Fail(
                AppError::WithCode(ErrorCode::kExternalServiceError, error));
        }

        auto state = std::make_shared<PublishState>();
        state->runtime = this;
        state->payload = payload;
        auto *state_holder = new std::shared_ptr<PublishState>(state);
        timeval timeout{};
        const int scheduled = event_base_once(
            base_.get(), -1, EV_TIMEOUT, PublishOnLoop, state_holder, &timeout);
        if (scheduled != 0) {
            delete state_holder;
            return VoidResult::Fail(
                AppError::WithCode(ErrorCode::kExternalServiceError,
                                   "rabbitmq publish scheduling failed"));
        }
        std::unique_lock<std::mutex> lock(state->mutex);
        if (!state->done.wait_for(lock, std::chrono::seconds(3),
                                  [&state]() { return state->completed; })) {
            return VoidResult::Fail(AppError::WithCode(
                ErrorCode::kExternalServiceError, "rabbitmq publish timeout"));
        }
        return state->result;
    }

  private:
    struct PublishState {
        AmqpPublisherRuntime *runtime = nullptr;
        std::string payload;
        std::mutex mutex;
        std::condition_variable done;
        bool completed = false;
        VoidResult result = VoidResult::Ok();
    };

    static void PublishOnLoop(evutil_socket_t, short, void *context) {
        auto *state_holder =
            static_cast<std::shared_ptr<PublishState> *>(context);
        auto state = *state_holder;
        delete state_holder;
        state->result = state->runtime->PublishOnLoop(state->payload);
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->completed = true;
        }
        state->done.notify_one();
    }

    VoidResult PublishOnLoop(const std::string &payload) {
        if (!channel_.publish(exchange_, routing_key_, payload,
                              AMQP::mandatory)) {
            return VoidResult::Fail(
                AppError::WithCode(ErrorCode::kExternalServiceError,
                                   "rabbitmq publish request write failed"));
        }
        std::lock_guard<std::mutex> error_lock(error_mutex_);
        if (!error_.empty()) {
            return VoidResult::Fail(
                AppError::WithCode(ErrorCode::kExternalServiceError, error_));
        }
        return VoidResult::Ok();
    }

    std::unique_ptr<event_base, EventBaseDeleter> base_;
    RuntimeHandler handler_;
    AMQP::Address address_;
    AMQP::TcpConnection connection_;
    AMQP::TcpChannel channel_;
    std::string exchange_;
    std::string queue_;
    std::string routing_key_;
    std::thread thread_;
    std::mutex error_mutex_;
    std::string error_;
};

class AmqpConsumerRuntime {
  public:
    using MessageHandler = ConfiguredMessageQueueConsumer::MessageHandler;

    AmqpConsumerRuntime(const RabbitmqConfig &config, MessageHandler handler,
                        std::size_t pool_size)
        : base_(CreateEventBase()), handler_(base_.get(), &config),
          address_(BuildRabbitmqAddress(config)),
          connection_(&handler_, address_), channel_(&connection_),
          exchange_(config.exchange), queue_(config.queue),
          routing_key_(config.routing_key),
          message_handler_(std::move(handler)), pool_size_(pool_size) {
        if (!base_) {
            return;
        }
        StartWorkerPool(pool_size);
        channel_.onError([](const char *message) {
            ZCHAT_LOG_ERROR("RabbitMQ consumer channel error: {}",
                            message == nullptr ? "unknown" : message);
        });
        channel_.declareExchange(exchange_, AMQP::direct, AMQP::durable);
        channel_.declareQueue(queue_, AMQP::durable);
        channel_.bindQueue(exchange_, queue_, routing_key_);
        channel_.setQos(
            static_cast<std::uint16_t>(pool_size > 0 ? pool_size * 2 : 8));
        channel_.consume(queue_).onReceived([this](const AMQP::Message &message,
                                                   std::uint64_t delivery_tag,
                                                   bool) {
            const std::string payload(message.body(), message.bodySize());
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                pending_tasks_.push({payload, delivery_tag});
            }
            queue_cv_.notify_one();
        });
        thread_ = std::thread([this]() { event_base_dispatch(base_.get()); });
    }

    AmqpConsumerRuntime(const AmqpConsumerRuntime &) = delete;
    AmqpConsumerRuntime &operator=(const AmqpConsumerRuntime &) = delete;
    AmqpConsumerRuntime(AmqpConsumerRuntime &&) = delete;
    AmqpConsumerRuntime &operator=(AmqpConsumerRuntime &&) = delete;

    ~AmqpConsumerRuntime() {
        if (!base_) {
            return;
        }
        StopWorkerPool();
        connection_.close();
        event_base_loopbreak(base_.get());
        if (thread_.joinable()) {
            thread_.join();
        }
    }

  private:
    struct Task {
        std::string payload;
        std::uint64_t delivery_tag = 0;
    };

    void StartWorkerPool(std::size_t pool_size) {
        stopping_.store(false);
        for (std::size_t i = 0; i < pool_size; ++i) {
            workers_.emplace_back([this]() { WorkerLoop(); });
        }
    }

    void StopWorkerPool() {
        stopping_.store(true);
        queue_cv_.notify_all();
        for (auto &worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        workers_.clear();
    }

    void WorkerLoop() {
        while (true) {
            Task task;
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                queue_cv_.wait(lock, [this]() {
                    return stopping_.load() || !pending_tasks_.empty();
                });
                if (stopping_.load() && pending_tasks_.empty()) {
                    return;
                }
                if (pending_tasks_.empty()) {
                    continue;
                }
                task = std::move(pending_tasks_.front());
                pending_tasks_.pop();
            }

            const auto handled = message_handler_(task.payload);
            if (!handled.ok()) {
                ZCHAT_LOG_ERROR(
                    "RabbitMQ message handling failed, requeueing: {}",
                    handled.error().message);
                EnqueueNack(task.delivery_tag);
            } else {
                EnqueueAck(task.delivery_tag);
            }
        }
    }

    void EnqueueNack(std::uint64_t delivery_tag) {
        {
            std::lock_guard<std::mutex> lock(ack_mutex_);
            pending_nacks_.push(delivery_tag);
        }
        timeval timeout{};
        event_base_once(base_.get(), -1, EV_TIMEOUT, DrainNacksCallback, this,
                        &timeout);
    }

    void EnqueueAck(std::uint64_t delivery_tag) {
        {
            std::lock_guard<std::mutex> lock(ack_mutex_);
            pending_acks_.push(delivery_tag);
        }
        timeval timeout{};
        event_base_once(base_.get(), -1, EV_TIMEOUT, DrainAcksCallback, this,
                        &timeout);
    }

    static void DrainAcksCallback(evutil_socket_t, short, void *context) {
        auto *self = static_cast<AmqpConsumerRuntime *>(context);
        self->DrainAcks();
    }

    void DrainAcks() {
        std::queue<std::uint64_t> acks;
        {
            std::lock_guard<std::mutex> lock(ack_mutex_);
            acks.swap(pending_acks_);
        }
        while (!acks.empty()) {
            channel_.ack(acks.front());
            acks.pop();
        }
    }

    static void DrainNacksCallback(evutil_socket_t, short, void *context) {
        auto *self = static_cast<AmqpConsumerRuntime *>(context);
        self->DrainNacks();
    }

    void DrainNacks() {
        std::queue<std::uint64_t> nacks;
        {
            std::lock_guard<std::mutex> lock(ack_mutex_);
            nacks.swap(pending_nacks_);
        }
        while (!nacks.empty()) {
            channel_.reject(nacks.front(), AMQP::requeue);
            nacks.pop();
        }
    }

    std::unique_ptr<event_base, EventBaseDeleter> base_;
    RuntimeHandler handler_;
    AMQP::Address address_;
    AMQP::TcpConnection connection_;
    AMQP::TcpChannel channel_;
    std::string exchange_;
    std::string queue_;
    std::string routing_key_;
    MessageHandler message_handler_;
    std::size_t pool_size_;
    std::thread thread_;

    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::queue<Task> pending_tasks_;
    std::atomic_bool stopping_{false};
    std::vector<std::thread> workers_;

    std::mutex ack_mutex_;
    std::queue<std::uint64_t> pending_acks_;
    std::queue<std::uint64_t> pending_nacks_;
};

ConfiguredMessageQueuePublisher::ConfiguredMessageQueuePublisher(
    const RabbitmqConfig &config)
    : exchange_(config.exchange), routing_key_(config.routing_key),
      runtime_(std::make_unique<AmqpPublisherRuntime>(config)) {}

ConfiguredMessageQueuePublisher::~ConfiguredMessageQueuePublisher() = default;

VoidResult
ConfiguredMessageQueuePublisher::Publish(const std::string &payload) {
    if (!runtime_) {
        return VoidResult::Fail(
            AppError::WithCode(ErrorCode::kExternalServiceError,
                               "rabbitmq publisher is not initialized"));
    }
    return runtime_->Publish(payload);
}

ConfiguredMessageQueueConsumer::ConfiguredMessageQueueConsumer(
    const RabbitmqConfig &config, MessageHandler handler, std::size_t pool_size)
    : runtime_(std::make_unique<AmqpConsumerRuntime>(config, std::move(handler),
                                                     pool_size)) {}

ConfiguredMessageQueueConsumer::~ConfiguredMessageQueueConsumer() = default;

std::string BuildRabbitmqAddress(const RabbitmqConfig &config) {
    const std::string scheme = config.tls.enable ? "amqps://" : "amqp://";
    return scheme + config.user + ":" + config.password + "@" + config.host +
           ":" + std::to_string(config.port) + "/";
}

} // namespace zchat
