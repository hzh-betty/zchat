#ifndef ZCHAT_SERVER_SRC_COMMON_RUNTIME_H_
#define ZCHAT_SERVER_SRC_COMMON_RUNTIME_H_

#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include <drogon/drogon.h>
#include <drogon/nosql/RedisClient.h>
#include <drogon/orm/DbClient.h>
#include <grpcpp/grpcpp.h>

#include "common/config.h"
#include "common/etcd_service.h"
#include "common/logger.h"

namespace zchat {

std::string ConfigPath(int argc, char *argv[], const std::string &default_path);
std::shared_ptr<drogon::orm::DbClient> MakeDbClient(const MysqlConfig &config);
drogon::nosql::RedisClientPtr MakeRedisClient(const RedisConfig &config);

template <typename GrpcService>
int RunGrpcServer(const std::string &service_name, int port,
                  GrpcService *service, const GrpcServerConfig &grpc_config,
                  const EtcdConfig *etcd_config = nullptr,
                  const std::string &logical_service_name = "") {
    InitLogger(service_name);
    try {
        const std::string address =
            grpc_config.bind_address + ":" + std::to_string(port);
        grpc::ServerBuilder builder;
        builder.AddListeningPort(address, grpc::InsecureServerCredentials());
        builder.RegisterService(service);
        builder.SetSyncServerOption(grpc::ServerBuilder::NUM_CQS,
                                    grpc_config.num_cqs);
        builder.SetSyncServerOption(grpc::ServerBuilder::MIN_POLLERS,
                                    grpc_config.min_pollers);
        builder.SetSyncServerOption(grpc::ServerBuilder::MAX_POLLERS,
                                    grpc_config.max_pollers);
        builder.SetSyncServerOption(grpc::ServerBuilder::CQ_TIMEOUT_MSEC,
                                    grpc_config.cq_timeout_msec);
        builder.SetMaxSendMessageSize(grpc_config.max_send_message_size);
        builder.SetMaxReceiveMessageSize(grpc_config.max_receive_message_size);
        std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
        if (server == nullptr) {
            ZCHAT_LOG_ERROR("{} failed: gRPC listen failed on {}", service_name,
                            address);
            return EXIT_FAILURE;
        }

        std::unique_ptr<EtcdRegistry> registry;
        if (etcd_config != nullptr && !logical_service_name.empty()) {
            registry = std::make_unique<EtcdRegistry>(*etcd_config);
            const auto registered = registry->Register(
                logical_service_name, BuildInstanceId(logical_service_name),
                BuildAdvertiseAddress(*etcd_config, port));
            if (!registered.ok()) {
                ZCHAT_LOG_ERROR("{} failed: {}", service_name,
                                FormatErrorForLog(registered.error()));
                return EXIT_FAILURE;
            }
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
