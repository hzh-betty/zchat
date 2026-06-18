#include "common/runtime.h"

#include <memory>
#include <netdb.h>
#include <stdexcept>
#include <sys/socket.h>

#include <trantor/net/InetAddress.h>

#include "common/logger.h"

namespace zchat {
namespace {

trantor::InetAddress ResolveAddress(const std::string &host, int port) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo *result = nullptr;
    const std::string service = std::to_string(port);
    const int error =
        getaddrinfo(host.c_str(), service.c_str(), &hints, &result);
    if (error != 0 || result == nullptr) {
        throw std::runtime_error("failed to resolve " + host + ":" + service +
                                 ": " + gai_strerror(error));
    }

    std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> resolved(result,
                                                                freeaddrinfo);
    for (addrinfo *item = resolved.get(); item != nullptr;
         item = item->ai_next) {
        if (item->ai_family == AF_INET) {
            return trantor::InetAddress(
                *reinterpret_cast<sockaddr_in *>(item->ai_addr));
        }
        if (item->ai_family == AF_INET6) {
            return trantor::InetAddress(
                *reinterpret_cast<sockaddr_in6 *>(item->ai_addr));
        }
    }

    throw std::runtime_error("failed to resolve " + host + ":" + service +
                             ": no usable address");
}

} // namespace

std::string ConfigPath(int argc, char *argv[],
                       const std::string &default_path) {
    if (argc > 1) {
        return argv[1];
    }
    return default_path;
}

std::shared_ptr<drogon::orm::DbClient> MakeDbClient(const MysqlConfig &config) {
    return drogon::orm::DbClient::newMysqlClient(
        BuildMysqlConnectionString(config), config.connections);
}

drogon::nosql::RedisClientPtr MakeRedisClient(const RedisConfig &config) {
    return drogon::nosql::RedisClient::newRedisClient(
        ResolveAddress(config.host, config.port), config.connections,
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
            .setMaxConnectionNum(10000)
            .setMaxConnectionNumPerIP(1000)
            .run();
        ZCHAT_LOG_INFO("{} stopped", service_name);
        return EXIT_SUCCESS;
    } catch (const std::exception &error) {
        ZCHAT_LOG_ERROR("{} failed: {}", service_name, error.what());
        return EXIT_FAILURE;
    }
}

} // namespace zchat
