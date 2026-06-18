#include "common/channel_pool.h"

#include <fstream>
#include <sstream>

#include <grpcpp/grpcpp.h>

#include "common/logger.h"

namespace zchat {
namespace {

std::string ReadFile(const std::string &path) {
    if (path.empty()) {
        return std::string();
    }
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        ZCHAT_LOG_WARN("channel_pool: cannot read file={}", path);
        return std::string();
    }
    std::ostringstream stream;
    stream << input.rdbuf();
    return stream.str();
}

std::shared_ptr<grpc::ChannelCredentials> MakeCredentials() {
    const char *ca = std::getenv("ZCHAT_GRPC_CA_PATH");
    const char *cert = std::getenv("ZCHAT_GRPC_CERT_PATH");
    const char *key = std::getenv("ZCHAT_GRPC_KEY_PATH");

    if (ca != nullptr && cert != nullptr && key != nullptr && *ca != '\0' &&
        *cert != '\0' && *key != '\0') {
        grpc::SslCredentialsOptions opts;
        opts.pem_root_certs = ReadFile(ca);
        opts.pem_cert_chain = ReadFile(cert);
        opts.pem_private_key = ReadFile(key);
        ZCHAT_LOG_INFO("channel_pool: using mTLS credentials ca={} cert={}", ca,
                       cert);
        return grpc::SslCredentials(opts);
    }
    return grpc::InsecureChannelCredentials();
}

} // namespace

std::shared_ptr<grpc::Channel>
ChannelPool::GetOrCreateChannel(const std::string &endpoint) {
    std::lock_guard<std::mutex> lock(channel_mutex_);
    auto it = channels_.find(endpoint);
    if (it != channels_.end()) {
        return it->second;
    }
    auto channel = grpc::CreateChannel(endpoint, MakeCredentials());
    channels_[endpoint] = channel;
    return channel;
}

} // namespace zchat
