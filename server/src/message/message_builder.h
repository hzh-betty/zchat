#ifndef ZCHAT_SERVER_SRC_MESSAGE_MESSAGE_BUILDER_H_
#define ZCHAT_SERVER_SRC_MESSAGE_MESSAGE_BUILDER_H_

#include "common/noncopyable.h"

#include <memory>

#include "common/config.h"
#include "message/message_context.h"

namespace zchat {

class MessageBuilder : public NonCopyable {
  public:
    explicit MessageBuilder(const AppConfig &config);

    ~MessageBuilder() = default;

    int Start();

  private:
    AppConfig config_;
    std::unique_ptr<MessageContext> context_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_MESSAGE_MESSAGE_BUILDER_H_
