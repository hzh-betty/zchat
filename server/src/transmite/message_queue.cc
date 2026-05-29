#include "transmite/message_queue.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

#include <amqpcpp.h>
#include <amqpcpp/libevent.h>
#include <event2/event.h>
#include <event2/thread.h>

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
    explicit RuntimeHandler(event_base *base) : AMQP::LibEventHandler(base) {}

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

    bool ready() const { return ready_.load(); }
    std::string error() const {
        std::lock_guard<std::mutex> lock(error_mutex_);
        return error_;
    }

  private:
    std::atomic_bool ready_{false};
    mutable std::mutex error_mutex_;
    std::string error_;
};

} // namespace

class AmqpPublisherRuntime {
  public:
    explicit AmqpPublisherRuntime(const RabbitmqConfig &config)
        : base_(CreateEventBase()), handler_(base_.get()),
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
            const std::string error =
                handler_error.empty() ? "rabbitmq connection is not ready"
                                      : handler_error;
            return VoidResult::Fail(AppError::WithCode(
                ErrorCode::kExternalServiceError, error));
        }

        PublishState state;
        state.runtime = this;
        state.payload = &payload;
        timeval timeout{};
        const int scheduled = event_base_once(base_.get(), -1, EV_TIMEOUT,
                                              PublishOnLoop, &state, &timeout);
        if (scheduled != 0) {
            return VoidResult::Fail(AppError::WithCode(
                ErrorCode::kExternalServiceError,
                "rabbitmq publish scheduling failed"));
        }
        std::unique_lock<std::mutex> lock(state.mutex);
        state.done.wait(lock, [&state]() { return state.completed; });
        return state.result;
    }

  private:
    struct PublishState {
        AmqpPublisherRuntime *runtime = nullptr;
        const std::string *payload = nullptr;
        std::mutex mutex;
        std::condition_variable done;
        bool completed = false;
        VoidResult result = VoidResult::Ok();
    };

    static void PublishOnLoop(evutil_socket_t, short, void *context) {
        auto *state = static_cast<PublishState *>(context);
        state->result = state->runtime->PublishOnLoop(*state->payload);
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->completed = true;
        }
        state->done.notify_one();
    }

    VoidResult PublishOnLoop(const std::string &payload) {
        if (!channel_.publish(exchange_, routing_key_, payload,
                              AMQP::mandatory)) {
            return VoidResult::Fail(AppError::WithCode(
                ErrorCode::kExternalServiceError,
                "rabbitmq publish request write failed"));
        }
        std::lock_guard<std::mutex> error_lock(error_mutex_);
        if (!error_.empty()) {
            return VoidResult::Fail(AppError::WithCode(
                ErrorCode::kExternalServiceError, error_));
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

    AmqpConsumerRuntime(const RabbitmqConfig &config, MessageHandler handler)
        : base_(CreateEventBase()), handler_(base_.get()),
          address_(BuildRabbitmqAddress(config)),
          connection_(&handler_, address_), channel_(&connection_),
          exchange_(config.exchange), queue_(config.queue),
          routing_key_(config.routing_key),
          message_handler_(std::move(handler)) {
        if (!base_) {
            return;
        }
        channel_.onError([](const char *message) {
            ZCHAT_LOG_ERROR("RabbitMQ consumer channel error: {}",
                            message == nullptr ? "unknown" : message);
        });
        channel_.declareExchange(exchange_, AMQP::direct, AMQP::durable);
        channel_.declareQueue(queue_, AMQP::durable);
        channel_.bindQueue(exchange_, queue_, routing_key_);
        channel_.consume(queue_).onReceived(
            [this](const AMQP::Message &message, std::uint64_t delivery_tag,
                   bool) {
                const std::string payload(message.body(), message.bodySize());
                const auto handled = message_handler_(payload);
                if (!handled.ok()) {
                    ZCHAT_LOG_ERROR("RabbitMQ message handling failed: {}",
                                    handled.error().message);
                }
                channel_.ack(delivery_tag);
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
        connection_.close();
        event_base_loopbreak(base_.get());
        if (thread_.joinable()) {
            thread_.join();
        }
    }

  private:
    std::unique_ptr<event_base, EventBaseDeleter> base_;
    RuntimeHandler handler_;
    AMQP::Address address_;
    AMQP::TcpConnection connection_;
    AMQP::TcpChannel channel_;
    std::string exchange_;
    std::string queue_;
    std::string routing_key_;
    MessageHandler message_handler_;
    std::thread thread_;
};

ConfiguredMessageQueuePublisher::ConfiguredMessageQueuePublisher(
    const RabbitmqConfig &config)
    : exchange_(config.exchange), routing_key_(config.routing_key),
      runtime_(std::make_unique<AmqpPublisherRuntime>(config)) {
}

ConfiguredMessageQueuePublisher::~ConfiguredMessageQueuePublisher() = default;

VoidResult
ConfiguredMessageQueuePublisher::Publish(const std::string &payload) {
    if (!runtime_) {
        return VoidResult::Fail(AppError::WithCode(
            ErrorCode::kExternalServiceError,
            "rabbitmq publisher is not initialized"));
    }
    return runtime_->Publish(payload);
}

ConfiguredMessageQueueConsumer::ConfiguredMessageQueueConsumer(
    const RabbitmqConfig &config, MessageHandler handler)
    : runtime_(
          std::make_unique<AmqpConsumerRuntime>(config, std::move(handler))) {
}

ConfiguredMessageQueueConsumer::~ConfiguredMessageQueueConsumer() = default;

std::string BuildRabbitmqAddress(const RabbitmqConfig &config) {
    return "amqp://" + config.user + ":" + config.password + "@" +
           config.host + ":" + std::to_string(config.port) + "/";
}

} // namespace zchat
