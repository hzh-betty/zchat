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
    static const int threads_enabled = evthread_use_pthreads();
    if (threads_enabled != 0) {
        return nullptr;
    }
    return event_base_new();
}

std::pair<std::string, std::uint16_t>
ParseRabbitmqHost(const std::string &host) {
    const auto colon = host.rfind(':');
    if (colon == std::string::npos) {
        return {host, 5672};
    }
    return {host.substr(0, colon),
            static_cast<std::uint16_t>(std::stoi(host.substr(colon + 1)))};
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
            return VoidResult::Fail("RabbitMQ event_base 初始化失败");
        }
        if (!handler_.ready()) {
            const std::string handler_error = handler_.error();
            const std::string error =
                handler_error.empty() ? "RabbitMQ 连接未就绪" : handler_error;
            return VoidResult::Fail(error);
        }

        PublishState state;
        state.runtime = this;
        state.payload = &payload;
        timeval timeout{};
        const int scheduled = event_base_once(base_.get(), -1, EV_TIMEOUT,
                                              PublishOnLoop, &state, &timeout);
        if (scheduled != 0) {
            return VoidResult::Fail("RabbitMQ 发布任务调度失败");
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
            return VoidResult::Fail("RabbitMQ 发布请求写入失败");
        }
        std::lock_guard<std::mutex> error_lock(error_mutex_);
        if (!error_.empty()) {
            return VoidResult::Fail(error_);
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

ConfiguredMessageQueuePublisher::ConfiguredMessageQueuePublisher(
    const RabbitmqConfig &config)
    : enabled_(config.enabled), exchange_(config.exchange),
      routing_key_(config.routing_key) {
    if (enabled_) {
        runtime_ = std::make_unique<AmqpPublisherRuntime>(config);
    }
}

ConfiguredMessageQueuePublisher::~ConfiguredMessageQueuePublisher() = default;

VoidResult
ConfiguredMessageQueuePublisher::Publish(const std::string &payload) {
    if (!enabled_) {
        return VoidResult::Ok();
    }
    if (!runtime_) {
        return VoidResult::Fail("RabbitMQ 发布器未初始化");
    }
    return runtime_->Publish(payload);
}

std::string BuildRabbitmqAddress(const RabbitmqConfig &config) {
    const auto [host, port] = ParseRabbitmqHost(config.host);
    return "amqp://" + config.user + ":" + config.password + "@" + host + ":" +
           std::to_string(port) + "/";
}

} // namespace zchat
