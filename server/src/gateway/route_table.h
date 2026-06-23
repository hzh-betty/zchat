#ifndef ZCHAT_SERVER_SRC_GATEWAY_ROUTE_TABLE_H_
#define ZCHAT_SERVER_SRC_GATEWAY_ROUTE_TABLE_H_

#include <chrono>
#include <functional>
#include <string>
#include <vector>

#include <drogon/HttpResponse.h>
#include <drogon/utils/coroutine.h>

namespace zchat {

class SessionStore;
class GrpcServiceClients;

struct RateLimitPolicy {
    const char *key_prefix;
    int window_seconds;
    int max_count;
};

struct RouteEntry {
    const char *path;
    const char *service_name;
    bool requires_auth;
    std::chrono::seconds deadline;
    std::function<drogon::Task<drogon::HttpResponsePtr>(
        SessionStore *, GrpcServiceClients &, const std::string &)>
        handle;
};

const RouteEntry *FindRoute(const std::string &path);
const std::vector<RouteEntry> &GetAllRoutes();

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_GATEWAY_ROUTE_TABLE_H_
