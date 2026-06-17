#ifndef ZCHAT_SERVER_SRC_COMMON_CHANNEL_POOL_H_
#define ZCHAT_SERVER_SRC_COMMON_CHANNEL_POOL_H_

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include <grpcpp/grpcpp.h>

#include "common/noncopyable.h"

namespace zchat {

class ChannelPool : public NonCopyable {
  public:
    ChannelPool() = default;
    ~ChannelPool() = default;

    std::shared_ptr<grpc::Channel>
    GetOrCreateChannel(const std::string &endpoint);

  private:
    std::mutex channel_mutex_;
    std::unordered_map<std::string, std::shared_ptr<grpc::Channel>> channels_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_COMMON_CHANNEL_POOL_H_
