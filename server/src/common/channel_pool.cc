#include "common/channel_pool.h"

namespace zchat {

std::shared_ptr<grpc::Channel>
ChannelPool::GetOrCreateChannel(const std::string &endpoint) {
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
