#ifndef ZCHAT_SERVER_SRC_COMMON_NOTIFY_PUBLISHER_H_
#define ZCHAT_SERVER_SRC_COMMON_NOTIFY_PUBLISHER_H_

#include "common/noncopyable.h"

#include <string>
#include <vector>

#include <drogon/nosql/RedisClient.h>
#include <drogon/utils/coroutine.h>

#include "common/result.h"

namespace zchat {

struct PublishOutcome {
    std::vector<std::string> succeeded;
    std::vector<std::string> failed;
};

class NotifyPublisher : public NonCopyable {
  public:
    explicit NotifyPublisher(drogon::nosql::RedisClientPtr redis);

    ~NotifyPublisher() = default;

    drogon::Task<VoidResult> PublishCoro(const std::string &user_id,
                                         const std::string &payload);
    drogon::Task<Result<PublishOutcome>>
    PublishBatchCoro(const std::vector<std::string> &user_ids,
                     const std::string &payload);

  private:
    drogon::nosql::RedisClientPtr redis_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_COMMON_NOTIFY_PUBLISHER_H_
