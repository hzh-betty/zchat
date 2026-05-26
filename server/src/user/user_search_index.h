#ifndef ZCHAT_SERVER_SRC_USER_USER_SEARCH_INDEX_H_
#define ZCHAT_SERVER_SRC_USER_USER_SEARCH_INDEX_H_

#include "common/noncopyable.h"

#include <memory>
#include <string>
#include <vector>

#include <drogon/HttpClient.h>
#include <trantor/net/EventLoopThread.h>

#include "common/config.h"
#include "common/domain_records.h"
#include "common/result.h"

namespace zchat {

class UserSearchIndex : public NonCopyable {
  public:
    UserSearchIndex() = default;
    virtual ~UserSearchIndex() = default;

    virtual VoidResult IndexUser(const UserRecord &user) = 0;
    virtual Result<std::vector<UserRecord>>
    SearchUsers(const std::string &keyword,
                const std::vector<std::string> &excluded_user_ids) = 0;
};

class ConfiguredUserSearchIndex final : public UserSearchIndex {
  public:
    explicit ConfiguredUserSearchIndex(const ElasticsearchConfig &config);
    ~ConfiguredUserSearchIndex() override = default;

    VoidResult IndexUser(const UserRecord &user) override;
    Result<std::vector<UserRecord>>
    SearchUsers(const std::string &keyword,
                const std::vector<std::string> &excluded_user_ids) override;

  private:
    VoidResult EnsureIndex();
    void AddAuthHeader(const drogon::HttpRequestPtr &request) const;

    std::string host_;
    std::string user_;
    std::string password_;
    std::unique_ptr<trantor::EventLoopThread> loop_thread_;
    drogon::HttpClientPtr client_;
};

std::string BuildElasticsearchUserDocument(const UserRecord &user);
std::string BuildElasticsearchUserSearchRequest(
    const std::string &keyword,
    const std::vector<std::string> &excluded_user_ids);
std::string BuildElasticsearchUserIndexDefinition();

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_USER_USER_SEARCH_INDEX_H_
