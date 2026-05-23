#ifndef ZCHAT_SERVER_SRC_USER_USER_CONTEXT_H_
#define ZCHAT_SERVER_SRC_USER_USER_CONTEXT_H_

#include "common/noncopyable.h"

#include <memory>

#include <drogon/nosql/RedisClient.h>
#include <drogon/orm/DbClient.h>

#include "common/config.h"
#include "common/session_store.h"
#include "file/file_repository.h"
#include "user/sms_client.h"
#include "user/user_grpc_service.h"
#include "user/user_repository.h"
#include "user/user_service.h"

namespace zchat {

class UserContext : public NonCopyable {
  public:
    explicit UserContext(const AppConfig &config);

    ~UserContext() = default;

    UserGrpcService &grpc_service() { return grpc_service_; }

  private:
    AppConfig config_;
    std::shared_ptr<drogon::orm::DbClient> db_;
    drogon::nosql::RedisClientPtr redis_;
    OrmUserRepository user_repository_;
    OrmFileRepository file_repository_;
    SessionStore sessions_;
    std::unique_ptr<SmsClient> sms_;
    std::shared_ptr<UserApplicationService> user_service_;
    UserGrpcService grpc_service_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_USER_USER_CONTEXT_H_