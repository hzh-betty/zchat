#ifndef ZCHAT_SERVER_SRC_FRIEND_FRIEND_BUILDER_H_
#define ZCHAT_SERVER_SRC_FRIEND_FRIEND_BUILDER_H_

#include "common/noncopyable.h"

#include <memory>

#include "common/config.h"
#include "friend/friend_context.h"

namespace zchat {

class FriendBuilder : public NonCopyable {
  public:
    explicit FriendBuilder(const AppConfig &config);

    ~FriendBuilder() = default;

    int Start();

  private:
    AppConfig config_;
    std::unique_ptr<FriendContext> context_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_FRIEND_FRIEND_BUILDER_H_
