#include "common/runtime.h"

#include <trantor/net/InetAddress.h>

#include "common/logger.h"

namespace zchat {

std::string ConfigPath(int argc, char *argv[]) {
    if (argc > 1) {
        return argv[1];
    }
    return "server/config/app.json";
}

std::shared_ptr<drogon::orm::DbClient> MakeDbClient(const MysqlConfig &config) {
    return drogon::orm::DbClient::newMysqlClient(
        BuildMysqlConnectionString(config), config.connections);
}

drogon::nosql::RedisClientPtr MakeRedisClient(const RedisConfig &config) {
    return drogon::nosql::RedisClient::newRedisClient(
        trantor::InetAddress(config.host, config.port), config.connections,
        config.password, config.database);
}

int RunDrogonGateway(const std::string &service_name, int http_port,
                     int websocket_port) {
    InitLogger(service_name);
    try {
        ZCHAT_LOG_INFO("{} listening http={} websocket={}", service_name,
                       http_port, websocket_port);
        drogon::app()
            .addListener("0.0.0.0", http_port)
            .addListener("0.0.0.0", websocket_port)
            .setThreadNum(4)
            .run();
        ZCHAT_LOG_INFO("{} stopped", service_name);
        return EXIT_SUCCESS;
    } catch (const std::exception &error) {
        ZCHAT_LOG_ERROR("{} failed: {}", service_name, error.what());
        return EXIT_FAILURE;
    }
}

} // namespace zchat
