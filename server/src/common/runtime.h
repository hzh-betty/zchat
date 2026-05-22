#ifndef ZCHAT_SERVER_SRC_COMMON_RUNTIME_H_
#define ZCHAT_SERVER_SRC_COMMON_RUNTIME_H_

#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <string>

#include <drogon/drogon.h>
#include <drogon/nosql/RedisClient.h>
#include <drogon/orm/DbClient.h>
#include <grpcpp/grpcpp.h>

#include "common/config.h"
#include "common/logger.h"

namespace zchat {

std::string ConfigPath(int argc, char *argv[]);
std::shared_ptr<drogon::orm::DbClient> MakeDbClient(const MysqlConfig &config);
drogon::nosql::RedisClientPtr MakeRedisClient(const RedisConfig &config);

template <typename GrpcService>
int RunGrpcServer(const std::string &service_name, int port,
                  GrpcService *service) {
    InitLogger(service_name);
    try {
        const std::string address = "0.0.0.0:" + std::to_string(port);
        grpc::ServerBuilder builder;
        builder.AddListeningPort(address, grpc::InsecureServerCredentials());
        builder.RegisterService(service);
        std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
        if (server == nullptr) {
            ZCHAT_LOG_ERROR("{} failed: gRPC listen failed on {}",
                            service_name, address);
            return EXIT_FAILURE;
        }
        ZCHAT_LOG_INFO("{} listening on {}", service_name, address);
        server->Wait();
        ZCHAT_LOG_INFO("{} stopped", service_name);
        return EXIT_SUCCESS;
    } catch (const std::exception &error) {
        ZCHAT_LOG_ERROR("{} failed: {}", service_name, error.what());
        return EXIT_FAILURE;
    }
}

int RunDrogonGateway(const std::string &service_name, int http_port,
                     int websocket_port);

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_COMMON_RUNTIME_H_
