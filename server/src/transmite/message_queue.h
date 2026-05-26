#ifndef ZCHAT_SERVER_SRC_TRANSMITE_MESSAGE_QUEUE_H_
#define ZCHAT_SERVER_SRC_TRANSMITE_MESSAGE_QUEUE_H_

#include "common/noncopyable.h"

#include <functional>
#include <memory>
#include <string>

#include "common/config.h"
#include "common/result.h"

namespace zchat {

class AmqpPublisherRuntime;
class AmqpConsumerRuntime;

class MessageQueuePublisher : public NonCopyable {
  public:
    MessageQueuePublisher() = default;
    virtual ~MessageQueuePublisher() = default;

    virtual VoidResult Publish(const std::string &payload) = 0;
};

class ConfiguredMessageQueuePublisher final : public MessageQueuePublisher {
  public:
    explicit ConfiguredMessageQueuePublisher(const RabbitmqConfig &config);
    ~ConfiguredMessageQueuePublisher() override;

    VoidResult Publish(const std::string &payload) override;

  private:
    std::string exchange_;
    std::string routing_key_;
    std::unique_ptr<AmqpPublisherRuntime> runtime_;
};

class ConfiguredMessageQueueConsumer final : public NonCopyable {
  public:
    using MessageHandler = std::function<VoidResult(const std::string &)>;

    ConfiguredMessageQueueConsumer(const RabbitmqConfig &config,
                                   MessageHandler handler);
    ~ConfiguredMessageQueueConsumer();

  private:
    std::unique_ptr<AmqpConsumerRuntime> runtime_;
};

std::string BuildRabbitmqAddress(const RabbitmqConfig &config);

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_TRANSMITE_MESSAGE_QUEUE_H_
