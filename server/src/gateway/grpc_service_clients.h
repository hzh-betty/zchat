#ifndef ZCHAT_SERVER_SRC_GATEWAY_GRPC_SERVICE_CLIENTS_H_
#define ZCHAT_SERVER_SRC_GATEWAY_GRPC_SERVICE_CLIENTS_H_

#include "common/noncopyable.h"

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include <grpcpp/grpcpp.h>

#include "common/config.h"
#include "common/etcd_service.h"

namespace zchat {

class GrpcServiceClients : public NonCopyable {
  public:
    explicit GrpcServiceClients(const AppConfig &config);

    ~GrpcServiceClients() = default;

    std::shared_ptr<grpc::Channel>
    GetOrCreateChannel(const std::string &endpoint);

    EtcdDiscovery &discovery() { return discovery_; }

  private:
    EtcdDiscovery discovery_;
    std::mutex channel_mutex_;
    std::unordered_map<std::string, std::shared_ptr<grpc::Channel>> channels_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_GATEWAY_GRPC_SERVICE_CLIENTS_H_
