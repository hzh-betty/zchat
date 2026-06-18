#ifndef ZCHAT_SERVER_SRC_COMMON_RUNTIME_H_
#define ZCHAT_SERVER_SRC_COMMON_RUNTIME_H_

#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
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

        // TLS/mTLS 服务端凭证
        const char *ca = std::getenv("ZCHAT_GRPC_CA_PATH");
        const char *cert = std::getenv("ZCHAT_GRPC_CERT_PATH");
        const char *key = std::getenv("ZCHAT_GRPC_KEY_PATH");
        if (ca != nullptr && cert != nullptr && key != nullptr && *ca != '\0' &&
            *cert != '\0' && *key != '\0') {
            std::ifstream ca_file(ca, std::ios::binary);
            std::ifstream cert_file(cert, std::ios::binary);
            std::ifstream key_file(key, std::ios::binary);
            std::ostringstream ca_stream, cert_stream, key_stream;
            ca_stream << ca_file.rdbuf();
            cert_stream << cert_file.rdbuf();
            key_stream << key_file.rdbuf();

            grpc::SslServerCredentialsOptions::PemKeyCertPair pkcp;
            pkcp.private_key = key_stream.str();
            pkcp.cert_chain = cert_stream.str();
            grpc::SslServerCredentialsOptions ssl_opts;
            ssl_opts.pem_root_certs = ca_stream.str();
            ssl_opts.pem_key_cert_pairs.push_back(pkcp);
            ssl_opts.force_client_auth = true; // mTLS
            builder.AddListeningPort(address,
                                     grpc::SslServerCredentials(ssl_opts));
            ZCHAT_LOG_INFO("{} using mTLS server credentials", service_name);
        } else {
            builder.AddListeningPort(address,
                                     grpc::InsecureServerCredentials());
        }
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
