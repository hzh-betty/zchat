#ifndef ZCHAT_SERVER_SRC_COMMON_CONFIG_H_
#define ZCHAT_SERVER_SRC_COMMON_CONFIG_H_

#include <cstddef>
#include <cstdint>
#include <string>

namespace zchat {

struct TlsConfig {
    bool enable = false;
    std::string ca_path;
    std::string cert_path;
    std::string key_path;
    std::string target_name_override;
};

struct LogConfig {
    std::string level = "debug";
    bool console = true;
    bool file = false;
    std::string file_path;
    std::size_t max_file_size = 52428800; // 50MB
    std::size_t max_files = 5;
    std::string format = "[%n][%Y-%m-%d %H:%M:%S.%e][%t][%-8l]%v";
};

struct MysqlConfig {
    std::string host = "127.0.0.1";
    int port = 3306;
    std::string database = "zchat";
    std::string user;
    std::string password;
    std::string charset = "utf8mb4";
    std::size_t connections = 4;
};

struct RedisConfig {
    std::string host = "127.0.0.1";
    int port = 6379;
    unsigned int database = 0;
    std::string password;
    std::size_t connections = 2;
};

struct ServerConfig {
    int http_port = 8000;
    int websocket_port = 8001;
    int thread_num = 4;
    int max_connections = 10000;
    int max_connections_per_ip = 1000;
    std::size_t client_max_body_size = 64 * 1024 * 1024;
};

struct ServicePortsConfig {
    int user = 10003;
    int file = 10002;
    int speech = 10001;
    int transmite = 10004;
    int message = 10005;
    int friend_service = 10006;
};

struct StorageConfig {
    std::string path = "./server_storage";
};

struct EtcdConfig {
    std::string endpoints = "http://127.0.0.1:2379";
    std::string base_path = "/service";
    std::string advertise_host = "127.0.0.1";
    std::string username;
    std::string password;
    int lease_ttl_seconds = 10;
    int auth_token_ttl_seconds = 300;
    TlsConfig tls;
};

struct SpeechConfig {
    std::string app_id;
    std::string api_key;
    std::string secret_key;
};

struct ElasticsearchConfig {
    std::string hosts = "http://127.0.0.1:9200/";
    std::string user;
    std::string password;
    TlsConfig tls;
};

struct RabbitmqConfig {
    std::string host = "127.0.0.1";
    int port = 5672;
    std::string user;
    std::string password;
    std::string exchange = "msg_exchange";
    std::string queue = "msg_queue";
    std::string routing_key = "msg_queue";
    TlsConfig tls;
};

struct GrpcServerConfig {
    std::string bind_address = "127.0.0.1";
    int num_cqs = 2;
    int min_pollers = 8;
    int max_pollers = 64;
    int cq_timeout_msec = 10000;
    int max_send_message_size = 4 * 1024 * 1024;
    int max_receive_message_size = 4 * 1024 * 1024;
    TlsConfig tls;
};

struct SmsConfig {
    std::string access_key_id;
    std::string access_key_secret;
    std::string sign_name;
    std::string template_code;
};

struct AppConfig {
    LogConfig log;
    ServerConfig server;
    ServicePortsConfig services;
    MysqlConfig mysql;
    RedisConfig redis;
    StorageConfig storage;
    EtcdConfig etcd;
    SpeechConfig speech;
    ElasticsearchConfig elasticsearch;
    RabbitmqConfig rabbitmq;
    GrpcServerConfig grpc;
    SmsConfig sms;
};

AppConfig LoadConfig(const std::string &path);
std::string BuildMysqlConnectionString(const MysqlConfig &config);

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_COMMON_CONFIG_H_
