#ifndef ZCHAT_SERVER_SRC_COMMON_ETCD_SERVICE_H_
#define ZCHAT_SERVER_SRC_COMMON_ETCD_SERVICE_H_

#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <etcd/Client.hpp>
#include <etcd/KeepAlive.hpp>
#include <etcd/Watcher.hpp>

#include "common/config.h"
#include "common/noncopyable.h"
#include "common/result.h"

namespace zchat {

class EtcdRegistry final : public NonCopyable {
  public:
    explicit EtcdRegistry(const EtcdConfig &config);
    ~EtcdRegistry();

    VoidResult Register(const std::string &service_name,
                        const std::string &instance_id,
                        const std::string &address);

  private:
    EtcdConfig config_;
    std::shared_ptr<etcd::Client> client_;
    std::shared_ptr<etcd::KeepAlive> keep_alive_;
    std::string registered_key_;
};

class EtcdDiscovery final : public NonCopyable {
  public:
    explicit EtcdDiscovery(const EtcdConfig &config);
    ~EtcdDiscovery();

    Result<std::string> Endpoint(const std::string &service_name);

  private:
    void LoadExisting();
    void Watch();
    void Upsert(const std::string &key, const std::string &address);
    void Remove(const std::string &key);

    EtcdConfig config_;
    std::shared_ptr<etcd::Client> client_;
    std::unique_ptr<etcd::Watcher> watcher_;
    std::mutex mutex_;
    std::unordered_map<std::string, std::string> key_to_service_;
    std::unordered_map<std::string, std::string> key_to_address_;
    std::unordered_map<std::string, std::vector<std::string>> service_keys_;
    std::unordered_map<std::string, std::size_t> next_index_;
};

std::string NormalizeEtcdBasePath(std::string path);
std::string EtcdServicePath(const EtcdConfig &config,
                            const std::string &service_name);
std::string EtcdInstanceKey(const EtcdConfig &config,
                            const std::string &service_name,
                            const std::string &instance_id);
std::string BuildAdvertiseAddress(const EtcdConfig &config, int port);
std::string BuildInstanceId(const std::string &service_name);

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_COMMON_ETCD_SERVICE_H_
