#ifndef ZCHAT_SERVER_SRC_COMMON_CONFIG_H_
#define ZCHAT_SERVER_SRC_COMMON_CONFIG_H_

#include <cstdint>
#include <string>

namespace zchat {

struct MysqlConfig {
    std::string host = "127.0.0.1";
    int port = 3306;
    std::string database = "zchat";
    std::string user = "testuser";
    std::string password = "123456";
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

struct SpeechConfig {
    std::string app_id;
    std::string api_key;
    std::string secret_key;
};

struct ElasticsearchConfig {
    std::string hosts = "http://127.0.0.1:9200/";
    std::string user = "elastic";
    std::string password;
};

struct RabbitmqConfig {
    std::string host = "127.0.0.1:5672";
    std::string user = "root";
    std::string password = "123456";
    std::string exchange = "msg_exchange";
    std::string queue = "msg_queue";
    std::string routing_key = "msg_queue";
};

struct SmsConfig {
    std::string access_key_id;
    std::string access_key_secret;
    std::string sign_name;
    std::string template_code;
};

struct AppConfig {
    ServerConfig server;
    ServicePortsConfig services;
    MysqlConfig mysql;
    RedisConfig redis;
    StorageConfig storage;
    SpeechConfig speech;
    ElasticsearchConfig elasticsearch;
    RabbitmqConfig rabbitmq;
    SmsConfig sms;
};

AppConfig LoadConfig(const std::string &path);
std::string BuildMysqlConnectionString(const MysqlConfig &config);

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_COMMON_CONFIG_H_
