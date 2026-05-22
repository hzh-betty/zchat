#ifndef ZCHAT_SERVER_SRC_USER_USER_BUILDER_H_
#define ZCHAT_SERVER_SRC_USER_USER_BUILDER_H_

#include "common/noncopyable.h"

#include <memory>

#include "common/config.h"
#include "user/user_context.h"

namespace zchat {

class UserBuilder : public NonCopyable {
  public:
    explicit UserBuilder(const AppConfig &config);

    ~UserBuilder() = default;

    int Start();

  private:
    AppConfig config_;
    std::unique_ptr<UserContext> context_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_USER_USER_BUILDER_H_
