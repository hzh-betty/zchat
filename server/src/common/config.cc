#include "common/config.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

namespace zchat {
using json = nlohmann::json;
namespace {

json ReadJsonFile(const std::string &path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        return json::object();
    }
    try {
        return json::parse(input);
    } catch (...) {
        return json::object();
    }
}

std::string TrimRight(std::string value) {
    value.erase(std::find_if(value.rbegin(), value.rend(),
                             [](unsigned char ch) { return !std::isspace(ch); })
                    .base(),
                value.end());
    return value;
}

std::string ReadSecretFile(const std::string &env_name) {
    const char *path = std::getenv(env_name.c_str());
    if (path == nullptr || *path == '\0') {
        return "";
    }
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return "";
    }
    std::ostringstream stream;
    stream << input.rdbuf();
    return TrimRight(stream.str());
}

std::string ResolvePlaceholder(const std::string &placeholder) {
    constexpr char kSecretFilePrefix[] = "secret_file:";
    const std::string prefix(kSecretFilePrefix);
    if (placeholder.rfind(prefix, 0) == 0) {
        return ReadSecretFile(placeholder.substr(prefix.size()));
    }

    const char *env_value = std::getenv(placeholder.c_str());
    if (env_value != nullptr) {
        return env_value;
    }
    return "";
}

std::string ResolveConfigValue(const std::string &value) {
    std::string resolved;
    std::size_t position = 0;
    while (position < value.size()) {
        const std::size_t begin = value.find("{{", position);
        if (begin == std::string::npos) {
            resolved.append(value, position, std::string::npos);
            break;
        }
        resolved.append(value, position, begin - position);
        const std::size_t end = value.find("}}", begin + 2);
        if (end == std::string::npos) {
            resolved.append(value, begin, std::string::npos);
            break;
        }
        const std::string placeholder =
            value.substr(begin + 2, end - begin - 2);
        resolved.append(ResolvePlaceholder(placeholder));
        position = end + 2;
    }
    return resolved;
}

std::string GetString(const json &value, const char *key,
                      const std::string &fallback) {
    if (!value.is_object() || !value.contains(key)) {
        return fallback;
    }
    return ResolveConfigValue(value[key].get<std::string>());
}

int GetInt(const json &value, const char *key, int fallback) {
    if (!value.is_object() || !value.contains(key)) {
        return fallback;
    }
    const auto &field = value[key];
    if (field.is_number_integer()) {
        return field.get<int>();
    }
    if (field.is_string()) {
        const std::string resolved =
            ResolveConfigValue(field.get<std::string>());
        if (resolved.empty()) {
            return fallback;
        }
        try {
            return std::stoi(resolved);
        } catch (...) {
            return fallback;
        }
    }
    return fallback;
}

std::size_t GetSize(const json &value, const char *key, std::size_t fallback) {
    if (!value.is_object() || !value.contains(key)) {
        return fallback;
    }
    return static_cast<std::size_t>(value[key].get<std::uint64_t>());
}

TlsConfig ReadTls(const json &value) {
    TlsConfig tls;
    if (!value.is_object()) {
        return tls;
    }
    if (value.contains("enable")) {
        if (value["enable"].is_boolean()) {
            tls.enable = value["enable"].get<bool>();
        } else if (value["enable"].is_number_integer()) {
            tls.enable = value["enable"].get<int>() != 0;
        }
    }
    tls.ca_path = GetString(value, "ca_path", tls.ca_path);
    tls.cert_path = GetString(value, "cert_path", tls.cert_path);
    tls.key_path = GetString(value, "key_path", tls.key_path);
    tls.target_name_override =
        GetString(value, "target_name_override", tls.target_name_override);
    return tls;
}

} // namespace

AppConfig LoadConfig(const std::string &path) {
    AppConfig config;
    const json root = ReadJsonFile(path);
    (void)root;

    const json server = root.value("server", json::object());
    config.server.http_port =
        GetInt(server, "http_port", config.server.http_port);
    config.server.websocket_port =
        GetInt(server, "websocket_port", config.server.websocket_port);

    const json services = root.value("services", json::object());
    config.services.user = GetInt(services, "user", config.services.user);
    config.services.file = GetInt(services, "file", config.services.file);
    config.services.speech = GetInt(services, "speech", config.services.speech);
    config.services.transmite =
        GetInt(services, "transmite", config.services.transmite);
    config.services.message =
        GetInt(services, "message", config.services.message);
    config.services.friend_service =
        GetInt(services, "friend", config.services.friend_service);

    const json mysql = root.value("mysql", json::object());
    config.mysql.host = GetString(mysql, "host", config.mysql.host);
    config.mysql.port = GetInt(mysql, "port", config.mysql.port);
    config.mysql.database = GetString(mysql, "database", config.mysql.database);
    config.mysql.user = GetString(mysql, "user", config.mysql.user);
    config.mysql.password = GetString(mysql, "password", config.mysql.password);
    config.mysql.charset = GetString(mysql, "charset", config.mysql.charset);
    config.mysql.connections =
        GetSize(mysql, "connections", config.mysql.connections);

    const json redis = root.value("redis", json::object());
    config.redis.host = GetString(redis, "host", config.redis.host);
    config.redis.port = GetInt(redis, "port", config.redis.port);
    config.redis.database =
        static_cast<unsigned int>(GetInt(redis, "database", 0));
    config.redis.password = GetString(redis, "password", config.redis.password);
    config.redis.connections =
        GetSize(redis, "connections", config.redis.connections);

    const json storage = root.value("storage", json::object());
    config.storage.path = GetString(storage, "path", config.storage.path);

    const json etcd = root.value("etcd", json::object());
    config.etcd.endpoints = GetString(etcd, "endpoints", config.etcd.endpoints);
    config.etcd.base_path = GetString(etcd, "base_path", config.etcd.base_path);
    config.etcd.advertise_host =
        GetString(etcd, "advertise_host", config.etcd.advertise_host);
    config.etcd.username = GetString(etcd, "username", config.etcd.username);
    config.etcd.password = GetString(etcd, "password", config.etcd.password);
    config.etcd.lease_ttl_seconds =
        GetInt(etcd, "lease_ttl_seconds", config.etcd.lease_ttl_seconds);
    config.etcd.auth_token_ttl_seconds = GetInt(
        etcd, "auth_token_ttl_seconds", config.etcd.auth_token_ttl_seconds);
    config.etcd.tls = ReadTls(etcd.value("tls", json::object()));

    const json speech = root.value("speech", json::object());
    config.speech.app_id = GetString(speech, "app_id", config.speech.app_id);
    config.speech.api_key = GetString(speech, "api_key", config.speech.api_key);
    config.speech.secret_key =
        GetString(speech, "secret_key", config.speech.secret_key);

    const json elasticsearch = root.value("elasticsearch", json::object());
    config.elasticsearch.hosts =
        GetString(elasticsearch, "hosts", config.elasticsearch.hosts);
    config.elasticsearch.user =
        GetString(elasticsearch, "user", config.elasticsearch.user);
    config.elasticsearch.password =
        GetString(elasticsearch, "password", config.elasticsearch.password);
    config.elasticsearch.tls =
        ReadTls(elasticsearch.value("tls", json::object()));

    const json rabbitmq = root.value("rabbitmq", json::object());
    config.rabbitmq.host = GetString(rabbitmq, "host", config.rabbitmq.host);
    config.rabbitmq.port = GetInt(rabbitmq, "port", config.rabbitmq.port);
    config.rabbitmq.user = GetString(rabbitmq, "user", config.rabbitmq.user);
    config.rabbitmq.password =
        GetString(rabbitmq, "password", config.rabbitmq.password);
    config.rabbitmq.exchange =
        GetString(rabbitmq, "exchange", config.rabbitmq.exchange);
    config.rabbitmq.queue = GetString(rabbitmq, "queue", config.rabbitmq.queue);
    config.rabbitmq.routing_key =
        GetString(rabbitmq, "routing_key", config.rabbitmq.routing_key);
    config.rabbitmq.tls = ReadTls(rabbitmq.value("tls", json::object()));

    const json grpc = root.value("grpc", json::object());
    config.grpc.bind_address =
        GetString(grpc, "bind_address", config.grpc.bind_address);
    config.grpc.num_cqs = GetInt(grpc, "num_cqs", config.grpc.num_cqs);
    config.grpc.min_pollers =
        GetInt(grpc, "min_pollers", config.grpc.min_pollers);
    config.grpc.max_pollers =
        GetInt(grpc, "max_pollers", config.grpc.max_pollers);
    config.grpc.cq_timeout_msec =
        GetInt(grpc, "cq_timeout_msec", config.grpc.cq_timeout_msec);
    config.grpc.max_send_message_size = GetInt(
        grpc, "max_send_message_size", config.grpc.max_send_message_size);
    config.grpc.max_receive_message_size = GetInt(
        grpc, "max_receive_message_size", config.grpc.max_receive_message_size);
    config.grpc.tls = ReadTls(grpc.value("tls", json::object()));

    const json sms = root.value("sms", json::object());
    config.sms.access_key_id =
        GetString(sms, "access_key_id", config.sms.access_key_id);
    config.sms.access_key_secret =
        GetString(sms, "access_key_secret", config.sms.access_key_secret);
    config.sms.sign_name = GetString(sms, "sign_name", config.sms.sign_name);
    config.sms.template_code =
        GetString(sms, "template_code", config.sms.template_code);
    return config;
}

std::string BuildMysqlConnectionString(const MysqlConfig &config) {
    std::ostringstream stream;
    stream << "host=" << config.host << " port=" << config.port
           << " dbname=" << config.database << " user=" << config.user
           << " password=" << config.password
           << " client_encoding=" << config.charset;
    return stream.str();
}

} // namespace zchat
