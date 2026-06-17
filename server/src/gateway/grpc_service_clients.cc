#include "gateway/grpc_service_clients.h"

#include <utility>

#include <grpcpp/grpcpp.h>

namespace zchat {

GrpcServiceClients::GrpcServiceClients(const AppConfig &config)
    : discovery_(config.etcd) {}

std::shared_ptr<grpc::Channel>
GrpcServiceClients::GetOrCreateChannel(const std::string &endpoint) {
    std::lock_guard<std::mutex> lock(channel_mutex_);
    auto it = channels_.find(endpoint);
    if (it != channels_.end()) {
        return it->second;
    }
    auto channel =
        grpc::CreateChannel(endpoint, grpc::InsecureChannelCredentials());
    channels_[endpoint] = channel;
    return channel;
}

} // namespace zchat
