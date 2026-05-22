#ifndef ZCHAT_SERVER_SRC_GATEWAY_GATEWAY_BUILDER_H_
#define ZCHAT_SERVER_SRC_GATEWAY_GATEWAY_BUILDER_H_

#include "common/noncopyable.h"

#include <memory>

#include "common/config.h"
#include "gateway/gateway_context.h"
#include "gateway/gateway_controller.h"

namespace zchat {

class GatewayBuilder : public NonCopyable {
  public:
    explicit GatewayBuilder(const AppConfig &config);

    ~GatewayBuilder() = default;

    int Start();

  private:
    AppConfig config_;
    std::shared_ptr<GatewayContext> context_;
    std::unique_ptr<GatewayController> controller_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_GATEWAY_GATEWAY_BUILDER_H_
