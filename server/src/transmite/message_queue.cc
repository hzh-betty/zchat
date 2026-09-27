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
#include <amqpcpp/reliable.h>
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
        if (tls_config_ != nullptr && tls_config_->tls.enable) {
            if (tls_config_->tls.ca_path.empty() || tls_config_->host.empty()) {
                return false;
            }
            SSL_CTX *ctx = SSL_get_SSL_CTX(ssl);
            if (ctx == nullptr ||
                SSL_CTX_load_verify_locations(
                    ctx, tls_config_->tls.ca_path.c_str(), nullptr) != 1 ||
                SSL_set1_host(ssl, tls_config_->host.c_str()) != 1) {
                return false;
            }
            SSL_set_verify(ssl, SSL_VERIFY_PEER, nullptr);
            if (tls_config_->tls.cert_path.empty() !=
                tls_config_->tls.key_path.empty()) {
                return false;
            }
            if (!tls_config_->tls.cert_path.empty() &&
                !tls_config_->tls.key_path.empty()) {
                if (SSL_CTX_use_certificate_file(
                        ctx, tls_config_->tls.cert_path.c_str(),
                        SSL_FILETYPE_PEM) != 1 ||
                    SSL_CTX_use_PrivateKey_file(
                        ctx, tls_config_->tls.key_path.c_str(),
                        SSL_FILETYPE_PEM) != 1 ||
                    SSL_CTX_check_private_key(ctx) != 1) {
                    return false;
                }
            }
        }
        return true;
    }

    bool onSecured(AMQP::TcpConnection *, const SSL *ssl) override {
        return SSL_get_verify_result(ssl) == X509_V_OK &&
               SSL_get0_peer_certificate(ssl) != nullptr;
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
          reliable_(channel_), exchange_(config.exchange), queue_(config.queue),
          routing_key_(config.routing_key) {
        if (!base_) {
            return;
        }
        channel_.onError([this](const char *message) {
            std::lock_guard<std::mutex> lock(error_mutex_);
            error_ =
                message == nullptr ? "unknown RabbitMQ channel error" : message;
        });
        channel_.recall().onReturned(
            [this](const AMQP::Message &, int16_t, const std::string &reason) {
                returned_generation_.fetch_add(1);
                ZCHAT_LOG_ERROR("RabbitMQ publish unroutable: {}", reason);
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
        if (!state->completed.wait_for(lock, std::chrono::seconds(5),
                                       [&state] { return state->done; })) {
            return VoidResult::Fail(AppError::WithCode(
                ErrorCode::kTimeout, "rabbitmq publisher confirm timed out"));
        }
        if (!state->success) {
            return VoidResult::Fail(AppError::WithCode(
                ErrorCode::kExternalServiceError,
                "rabbitmq did not confirm message publication"));
        }
        return VoidResult::Ok();
    }

  private:
    struct PublishState {
        AmqpPublisherRuntime *runtime = nullptr;
        std::string payload;
        std::mutex mutex;
        std::condition_variable completed;
        bool done = false;
        bool success = false;

        void Complete(bool confirmed) {
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (done) {
                    return;
                }
                done = true;
                success = confirmed;
            }
            completed.notify_one();
        }
    };

    static void PublishOnLoop(evutil_socket_t, short, void *context) {
        auto *state_holder =
            static_cast<std::shared_ptr<PublishState> *>(context);
        auto state = *state_holder;
        delete state_holder;
        state->runtime->PublishOnLoop(state);
    }

    void PublishOnLoop(const std::shared_ptr<PublishState> &state) {
        AMQP::Envelope envelope(state->payload.data(), state->payload.size());
        envelope.setDeliveryMode(2);
        const auto generation = returned_generation_.load();
        reliable_.publish(exchange_, routing_key_, envelope, AMQP::mandatory)
            .onAck([this, state, generation]() {
                state->Complete(returned_generation_.load() == generation);
            })
            .onNack([state]() { state->Complete(false); })
            .onLost([state]() { state->Complete(false); });
    }

    std::unique_ptr<event_base, EventBaseDeleter> base_;
    RuntimeHandler handler_;
    AMQP::Address address_;
    AMQP::TcpConnection connection_;
    AMQP::TcpChannel channel_;
    AMQP::Reliable<> reliable_;
    std::atomic<std::uint64_t> returned_generation_{0};
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
          republisher_(channel_), exchange_(config.exchange),
          queue_(config.queue), routing_key_(config.routing_key),
          message_handler_(std::move(handler)), pool_size_(pool_size) {
        if (!base_) {
            return;
        }
        StartWorkerPool(pool_size);
        republisher_.onError([this](const char *message) {
            parking_failed_ = true;
            ZCHAT_LOG_ERROR("RabbitMQ consumer channel error: {}",
                            message == nullptr ? "unknown" : message);
        });
        channel_.recall().onReturned(
            [this](const AMQP::Message &, int16_t, const std::string &reason) {
                parking_failed_ = true;
                ZCHAT_LOG_ERROR(
                    "RabbitMQ storage retry/blocked message unroutable: {}",
                    reason);
                connection_.close();
            });
        channel_.declareExchange(exchange_, AMQP::direct, AMQP::durable);
        channel_.declareQueue(queue_, AMQP::durable);
        channel_.declareQueue(queue_ + ".storage_blocked", AMQP::durable);
        AMQP::Table retry_arguments;
        retry_arguments["x-message-ttl"] = 60000;
        retry_arguments["x-dead-letter-exchange"] = "";
        retry_arguments["x-dead-letter-routing-key"] = queue_;
        channel_.declareQueue(queue_ + ".storage_retry", AMQP::durable,
                              retry_arguments);
        channel_.bindQueue(exchange_, queue_, routing_key_);
        channel_.setQos(
            static_cast<std::uint16_t>(pool_size > 0 ? pool_size * 2 : 8));
        channel_.consume(queue_).onReceived([this](const AMQP::Message &message,
                                                   std::uint64_t delivery_tag,
                                                   bool) {
            const std::string payload(message.body(), message.bodySize());
            int retry_count = 0;
            if (message.hasHeaders() &&
                message.headers().contains("x-zchat-retry")) {
                retry_count = static_cast<int32_t>(
                    message.headers().get("x-zchat-retry"));
            }
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                pending_tasks_.push({payload, delivery_tag, {}, retry_count});
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
        std::string parking_queue;
        int retry_count = 0;
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
                const bool retry =
                    handled.error().code != ErrorCode::kFileStorageRejected &&
                    handled.error().code != ErrorCode::kInvalidArgument &&
                    task.retry_count < 5;
                task.parking_queue =
                    queue_ + (retry ? ".storage_retry" : ".storage_blocked");
                if (retry) {
                    ++task.retry_count;
                }
                ZCHAT_LOG_WARN(
                    "RabbitMQ message handling failed, {} "
                    "attempt={}: {}",
                    retry ? "retry in 60 seconds" : "park for operator review",
                    task.retry_count, FormatErrorForLog(handled.error()));
                {
                    std::lock_guard<std::mutex> lock(ack_mutex_);
                    pending_parked_.push(std::move(task));
                }
                timeval timeout{};
                event_base_once(base_.get(), -1, EV_TIMEOUT,
                                DrainParkedCallback, this, &timeout);
            } else {
                EnqueueAck(task.delivery_tag);
            }
        }
    }

    static void DrainParkedCallback(evutil_socket_t, short, void *context) {
        auto *self = static_cast<AmqpConsumerRuntime *>(context);
        std::queue<Task> tasks;
        {
            std::lock_guard<std::mutex> lock(self->ack_mutex_);
            tasks.swap(self->pending_parked_);
        }
        while (!tasks.empty()) {
            auto task = std::move(tasks.front());
            tasks.pop();
            AMQP::Envelope envelope(task.payload.data(), task.payload.size());
            envelope.setDeliveryMode(2);
            AMQP::Table headers;
            headers.set("x-zchat-retry",
                        static_cast<int32_t>(task.retry_count));
            envelope.setHeaders(std::move(headers));
            self->republisher_
                .publish("", task.parking_queue, envelope, AMQP::mandatory)
                .onAck([self, tag = task.delivery_tag]() {
                    // Keep the source unacked unless durable publication
                    // succeeded.
                    if (!self->parking_failed_)
                        self->channel_.ack(tag);
                })
                .onLost([self]() {
                    self->parking_failed_ = true;
                    ZCHAT_LOG_ERROR(
                        "RabbitMQ failed to preserve storage-blocked message; "
                        "source remains unacked");
                    self->connection_.close();
                })
                .onNack([self]() {
                    self->parking_failed_ = true;
                    ZCHAT_LOG_ERROR("RabbitMQ rejected retry publication");
                    self->connection_.close();
                });
        }
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

    std::unique_ptr<event_base, EventBaseDeleter> base_;
    RuntimeHandler handler_;
    AMQP::Address address_;
    AMQP::TcpConnection connection_;
    AMQP::TcpChannel channel_;
    AMQP::Reliable<> republisher_;
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
    std::queue<Task> pending_parked_;
    bool parking_failed_ = false;
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
