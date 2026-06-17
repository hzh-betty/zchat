#include "gateway/grpc_service_clients.h"

namespace zchat {

GrpcServiceClients::GrpcServiceClients(const AppConfig &config)
    : discovery_(config.etcd) {}

} // namespace zchat
