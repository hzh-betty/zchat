#ifndef ZCHAT_SERVER_SRC_COMMON_SEARCH_MESSAGE_SEARCH_INDEX_H_
#define ZCHAT_SERVER_SRC_COMMON_SEARCH_MESSAGE_SEARCH_INDEX_H_

#include "common/noncopyable.h"

#include <memory>
#include <string>
#include <vector>

#include <drogon/HttpClient.h>
#include <drogon/utils/coroutine.h>
#include <trantor/net/EventLoopThread.h>

#include "common/config.h"
#include "common/domain_records.h"
#include "common/result.h"

namespace zchat {

class MessageSearchIndex : public NonCopyable {
  public:
    MessageSearchIndex() = default;
    virtual ~MessageSearchIndex() = default;

    virtual drogon::Task<VoidResult>
    IndexMessageCoro(const MessageRecord &message) = 0;
    virtual drogon::Task<Result<std::vector<MessageRecord>>>
    SearchMessagesCoro(const std::string &session_id,
                       const std::string &keyword, int offset, int limit) = 0;
};

class ConfiguredMessageSearchIndex final : public MessageSearchIndex {
  public:
    explicit ConfiguredMessageSearchIndex(const ElasticsearchConfig &config);
    ~ConfiguredMessageSearchIndex() override = default;

    drogon::Task<VoidResult>
    IndexMessageCoro(const MessageRecord &message) override;
    drogon::Task<Result<std::vector<MessageRecord>>>
    SearchMessagesCoro(const std::string &session_id,
                       const std::string &keyword, int offset,
                       int limit) override;

  private:
    VoidResult EnsureIndex();
    void AddAuthHeader(const drogon::HttpRequestPtr &request) const;

    std::string host_;
    std::string user_;
    std::string password_;
    std::unique_ptr<trantor::EventLoopThread> loop_thread_;
    drogon::HttpClientPtr client_;
};

std::string BuildElasticsearchMessageDocument(const MessageRecord &message);
std::string BuildElasticsearchSearchRequest(const std::string &session_id,
                                            const std::string &keyword,
                                            int offset, int limit);
std::string BuildElasticsearchMessageIndexDefinition();

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_COMMON_SEARCH_MESSAGE_SEARCH_INDEX_H_
