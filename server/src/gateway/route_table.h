#ifndef ZCHAT_SERVER_SRC_GATEWAY_ROUTE_TABLE_H_
#define ZCHAT_SERVER_SRC_GATEWAY_ROUTE_TABLE_H_

#include <chrono>
#include <functional>
#include <string>
#include <vector>

#include <drogon/HttpResponse.h>

namespace zchat {

class SessionStore;
class GrpcServiceClients;

struct RouteEntry {
    const char *path;
    const char *service_name;
    bool requires_auth;
    std::chrono::seconds deadline;
    std::function<void(SessionStore *, GrpcServiceClients &,
                       const std::string &,
                       std::function<void(const drogon::HttpResponsePtr &)> &&)>
        handle;
};

const RouteEntry *FindRoute(const std::string &path);
const std::vector<RouteEntry> &GetAllRoutes();

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_GATEWAY_ROUTE_TABLE_H_
