#include "common/etcd_service.h"

#include <algorithm>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <utility>

#include <unistd.h>

#include <etcd/Response.hpp>
#include <etcd/Value.hpp>

#include "common/common_errors.h"
#include "common/logger.h"

namespace zchat {
namespace {

std::shared_ptr<etcd::Client> MakeEtcdClient(const EtcdConfig &config) {
    const bool has_user = !config.username.empty();
    const bool has_password = !config.password.empty();
    if (has_user != has_password) {
        throw std::invalid_argument(
            "etcd username and password must be configured together");
    }
    if (config.tls.enable && !config.tls.ca_path.empty()) {
        return std::shared_ptr<etcd::Client>(etcd::Client::WithSSL(
            config.endpoints, config.tls.ca_path, config.tls.cert_path,
            config.tls.key_path, config.tls.target_name_override,
            "round_robin"));
    }
    if (has_user) {
        return std::make_shared<etcd::Client>(config.endpoints, config.username,
                                              config.password,
                                              config.auth_token_ttl_seconds);
    }
    return std::make_shared<etcd::Client>(config.endpoints);
}

std::shared_ptr<etcd::KeepAlive>
MakeKeepAlive(const EtcdConfig &config,
              const std::shared_ptr<etcd::Client> &client) {
    if (!config.username.empty()) {
        return std::make_shared<etcd::KeepAlive>(
            config.endpoints, config.username, config.password,
            config.lease_ttl_seconds, 0, config.auth_token_ttl_seconds);
    }
    return std::make_shared<etcd::KeepAlive>(*client, config.lease_ttl_seconds);
}

std::unique_ptr<etcd::Watcher>
MakeWatcher(const EtcdConfig &config,
            const std::shared_ptr<etcd::Client> &client,
            const std::string &base_path,
            const std::function<void(etcd::Response)> &callback) {
    if (!config.username.empty()) {
        return std::make_unique<etcd::Watcher>(
            config.endpoints, config.username, config.password, base_path,
            callback, true, config.auth_token_ttl_seconds);
    }
    return std::make_unique<etcd::Watcher>(*client, base_path, callback, true);
}

std::optional<std::string> ServiceNameFromKey(const EtcdConfig &config,
                                              const std::string &key) {
    const std::string prefix = NormalizeEtcdBasePath(config.base_path) + "/";
    if (key.rfind(prefix, 0) != 0) {
        return std::nullopt;
    }
    const std::string suffix = key.substr(prefix.size());
    const auto slash = suffix.find('/');
    if (slash == std::string::npos || slash == 0) {
        return std::nullopt;
    }
    return suffix.substr(0, slash);
}

AppError EtcdError(std::string message, const etcd::Response &response) {
    return AppError::WithCode(ErrorCode::kExternalServiceError,
                              std::move(message))
        .WithContext("provider_code", std::to_string(response.error_code()))
        .WithDetail(response.error_message());
}

} // namespace

std::string NormalizeEtcdBasePath(std::string path) {
    if (path.empty()) {
        path = "/service";
    }
    if (path.front() != '/') {
        path.insert(path.begin(), '/');
    }
    while (path.size() > 1 && path.back() == '/') {
        path.pop_back();
    }
    return path;
}

std::string EtcdServicePath(const EtcdConfig &config,
                            const std::string &service_name) {
    return NormalizeEtcdBasePath(config.base_path) + "/" + service_name;
}

std::string EtcdInstanceKey(const EtcdConfig &config,
                            const std::string &service_name,
                            const std::string &instance_id) {
    return EtcdServicePath(config, service_name) + "/" + instance_id;
}

std::string BuildAdvertiseAddress(const EtcdConfig &config, int port) {
    const std::string host = config.advertise_host.empty()
                                 ? std::string("127.0.0.1")
                                 : config.advertise_host;
    return host + ":" + std::to_string(port);
}

std::string BuildInstanceId(const std::string &service_name) {
    return service_name + "-" +
           std::to_string(static_cast<long long>(getpid()));
}

EtcdRegistry::EtcdRegistry(const EtcdConfig &config)
    : config_(config), client_(MakeEtcdClient(config)),
      keep_alive_(MakeKeepAlive(config_, client_)) {}

EtcdRegistry::~EtcdRegistry() {
    if (keep_alive_) {
        keep_alive_->Cancel();
    }
}

VoidResult EtcdRegistry::Register(const std::string &service_name,
                                  const std::string &instance_id,
                                  const std::string &address) {
    if (!keep_alive_) {
        return VoidResult::Fail(
            AppError::WithCode(ErrorCode::kExternalServiceError,
                               "etcd keepalive is not initialized"));
    }
    registered_key_ = EtcdInstanceKey(config_, service_name, instance_id);
    const auto response =
        client_->put(registered_key_, address, keep_alive_->Lease()).get();
    if (!response.is_ok()) {
        return VoidResult::Fail(
            EtcdError("etcd service registration failed", response));
    }
    ZCHAT_LOG_INFO("etcd registered service={} key={} address={}", service_name,
                   registered_key_, address);
    return VoidResult::Ok();
}

EtcdDiscovery::EtcdDiscovery(const EtcdConfig &config)
    : config_(config), client_(MakeEtcdClient(config)) {
    config_.base_path = NormalizeEtcdBasePath(config_.base_path);
    LoadExisting();
    Watch();
}

EtcdDiscovery::~EtcdDiscovery() {
    if (watcher_) {
        watcher_->Cancel();
    }
}

Result<std::string> EtcdDiscovery::Endpoint(const std::string &service_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto keys = service_keys_.find(service_name);
    if (keys == service_keys_.end() || keys->second.empty()) {
        return Result<std::string>::Fail(
            common_errors::ServiceEndpointNotFound().WithContext("service",
                                                                 service_name));
    }
    std::size_t &next = next_index_[service_name];
    if (next >= keys->second.size()) {
        next = 0;
    }
    const std::string key = keys->second[next++];
    auto address = key_to_address_.find(key);
    if (address == key_to_address_.end() || address->second.empty()) {
        return Result<std::string>::Fail(
            common_errors::ServiceEndpointNotFound().WithContext("service",
                                                                 service_name));
    }
    return Result<std::string>::Ok(address->second);
}

void EtcdDiscovery::LoadExisting() {
    const auto response = client_->ls(config_.base_path).get();
    if (!response.is_ok()) {
        ZCHAT_LOG_WARN("etcd discovery initial load failed: {}",
                       response.error_message());
        return;
    }
    for (std::size_t i = 0; i < response.keys().size(); ++i) {
        Upsert(response.key(static_cast<int>(i)),
               response.value(static_cast<int>(i)).as_string());
    }
}

void EtcdDiscovery::Watch() {
    watcher_ = MakeWatcher(
        config_, client_, config_.base_path, [this](etcd::Response response) {
            if (!response.is_ok()) {
                ZCHAT_LOG_WARN("etcd discovery watch failed: {}",
                               response.error_message());
                return;
            }
            for (const auto &event : response.events()) {
                if (event.event_type() == etcd::Event::EventType::PUT &&
                    event.has_kv()) {
                    Upsert(event.kv().key(), event.kv().as_string());
                } else if (event.event_type() ==
                           etcd::Event::EventType::DELETE_) {
                    if (event.has_prev_kv()) {
                        Remove(event.prev_kv().key());
                    } else if (event.has_kv()) {
                        Remove(event.kv().key());
                    }
                }
            }
        });
}

void EtcdDiscovery::Upsert(const std::string &key, const std::string &address) {
    auto service = ServiceNameFromKey(config_, key);
    if (!service.has_value() || address.empty()) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    key_to_service_[key] = *service;
    key_to_address_[key] = address;
    auto &keys = service_keys_[*service];
    if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
        keys.push_back(key);
    }
    ZCHAT_LOG_INFO("etcd discovered service={} key={} address={}", *service,
                   key, address);
}

void EtcdDiscovery::Remove(const std::string &key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto service = key_to_service_.find(key);
    if (service == key_to_service_.end()) {
        return;
    }
    auto keys = service_keys_.find(service->second);
    if (keys != service_keys_.end()) {
        keys->second.erase(
            std::remove(keys->second.begin(), keys->second.end(), key),
            keys->second.end());
    }
    ZCHAT_LOG_INFO("etcd removed service={} key={}", service->second, key);
    key_to_address_.erase(key);
    key_to_service_.erase(service);
}

} // namespace zchat
