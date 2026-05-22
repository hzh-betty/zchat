#ifndef ZCHAT_SERVER_SRC_GATEWAY_GATEWAY_CONTROLLER_H_
#define ZCHAT_SERVER_SRC_GATEWAY_GATEWAY_CONTROLLER_H_

#include "common/noncopyable.h"

#include <memory>

#include <drogon/HttpAppFramework.h>

#include "gateway/gateway_context.h"

namespace zchat {

class GatewayController : public NonCopyable {
  public:
    explicit GatewayController(std::shared_ptr<GatewayContext> context);

    ~GatewayController() = default;

    void RegisterRoutes();

  private:
    void RegisterForwardPost(const std::string &path,
                             const std::string &service_name);

    std::shared_ptr<GatewayContext> context_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_GATEWAY_GATEWAY_CONTROLLER_H_
