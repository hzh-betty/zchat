#include "message/message_search_index.h"

#include <sstream>
#include <utility>

#include <drogon/HttpRequest.h>
#include <json/json.h>

#include "common/crypto.h"
#include "common/logger.h"

namespace zchat {
namespace {

constexpr char kIndexPath[] = "/zchat_messages";
constexpr double kRequestTimeoutSeconds = 3.0;

std::string FirstHost(std::string hosts) {
    const auto comma = hosts.find(',');
    if (comma != std::string::npos) {
        hosts = hosts.substr(0, comma);
    }
    while (!hosts.empty() && hosts.back() == '/') {
        hosts.pop_back();
    }
    return hosts;
}

std::string CompactJson(const Json::Value &value) {
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, value);
}

MessageRecord MessageRecordFromJson(const Json::Value &source) {
    MessageRecord message;
    message.message_id = source["message_id"].asString();
    message.session_id = source["session_id"].asString();
    message.user_id = source["user_id"].asString();
    message.message_type = source["message_type"].asInt();
    message.create_time = source["create_time"].asInt64();
    message.content = source["content"].asString();
    message.file_id = source["file_id"].asString();
    message.file_name = source["file_name"].asString();
    message.file_size = source["file_size"].asUInt64();
    return message;
}

Result<std::vector<MessageRecord>>
ParseSearchResponse(const std::string &body) {
    Json::CharReaderBuilder builder;
    Json::Value root;
    std::string errors;
    std::istringstream input(body);
    if (!Json::parseFromStream(builder, input, &root, &errors)) {
        return Result<std::vector<MessageRecord>>::Fail(
            "Elasticsearch 搜索响应解析失败: " + errors);
    }

    std::vector<MessageRecord> messages;
    const Json::Value hits = root["hits"]["hits"];
    if (!hits.isArray()) {
        return Result<std::vector<MessageRecord>>::Ok(std::move(messages));
    }
    for (const auto &hit : hits) {
        messages.push_back(MessageRecordFromJson(hit["_source"]));
    }
    return Result<std::vector<MessageRecord>>::Ok(std::move(messages));
}

bool IsSuccessStatus(drogon::HttpStatusCode status) {
    return status >= drogon::k200OK && status < drogon::k300MultipleChoices;
}

} // namespace

ConfiguredMessageSearchIndex::ConfiguredMessageSearchIndex(
    const ElasticsearchConfig &config)
    : enabled_(config.enabled), host_(FirstHost(config.hosts)),
      user_(config.user), password_(config.password) {
    if (!enabled_) {
        return;
    }
    loop_thread_ =
        std::make_unique<trantor::EventLoopThread>("zchat-es-http-client");
    loop_thread_->run();
    client_ = drogon::HttpClient::newHttpClient(host_, loop_thread_->getLoop());
}

VoidResult
ConfiguredMessageSearchIndex::IndexMessage(const MessageRecord &message) {
    if (!enabled_) {
        return VoidResult::Ok();
    }
    if (!client_) {
        return VoidResult::Fail("Elasticsearch 客户端未初始化");
    }

    auto request = drogon::HttpRequest::newHttpRequest();
    request->setMethod(drogon::Put);
    request->setPath(std::string(kIndexPath) + "/_doc/" + message.message_id);
    request->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    request->setBody(BuildElasticsearchMessageDocument(message));
    AddAuthHeader(request);

    const auto [result, response] =
        client_->sendRequest(request, kRequestTimeoutSeconds);
    if (result != drogon::ReqResult::Ok || !response) {
        return VoidResult::Fail("Elasticsearch 索引请求失败: " +
                                drogon::to_string(result));
    }
    if (!IsSuccessStatus(response->statusCode())) {
        return VoidResult::Fail("Elasticsearch 索引返回异常状态: " +
                                std::to_string(response->statusCode()));
    }
    return VoidResult::Ok();
}

Result<std::vector<MessageRecord>>
ConfiguredMessageSearchIndex::SearchMessages(const std::string &session_id,
                                             const std::string &keyword) {
    if (!enabled_) {
        return Result<std::vector<MessageRecord>>::Fail("Elasticsearch 未启用");
    }
    if (!client_) {
        return Result<std::vector<MessageRecord>>::Fail(
            "Elasticsearch 客户端未初始化");
    }

    auto request = drogon::HttpRequest::newHttpRequest();
    request->setMethod(drogon::Post);
    request->setPath(std::string(kIndexPath) + "/_search");
    request->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    request->setBody(BuildElasticsearchSearchRequest(session_id, keyword));
    AddAuthHeader(request);

    const auto [result, response] =
        client_->sendRequest(request, kRequestTimeoutSeconds);
    if (result != drogon::ReqResult::Ok || !response) {
        return Result<std::vector<MessageRecord>>::Fail(
            "Elasticsearch 搜索请求失败: " + drogon::to_string(result));
    }
    if (!IsSuccessStatus(response->statusCode())) {
        return Result<std::vector<MessageRecord>>::Fail(
            "Elasticsearch 搜索返回异常状态: " +
            std::to_string(response->statusCode()));
    }
    return ParseSearchResponse(std::string(response->body()));
}

std::string BuildElasticsearchMessageDocument(const MessageRecord &message) {
    Json::Value root(Json::objectValue);
    root["message_id"] = message.message_id;
    root["session_id"] = message.session_id;
    root["user_id"] = message.user_id;
    root["message_type"] = message.message_type;
    root["create_time"] = Json::Int64(message.create_time);
    root["content"] = message.content;
    root["file_id"] = message.file_id;
    root["file_name"] = message.file_name;
    root["file_size"] = Json::UInt64(message.file_size);
    return CompactJson(root);
}

std::string BuildElasticsearchSearchRequest(const std::string &session_id,
                                            const std::string &keyword) {
    Json::Value root(Json::objectValue);
    root["size"] = 100;
    root["sort"][0]["create_time"]["order"] = "asc";

    Json::Value filters(Json::arrayValue);
    Json::Value session_filter(Json::objectValue);
    session_filter["term"]["session_id.keyword"] = session_id;
    filters.append(session_filter);
    Json::Value type_filter(Json::objectValue);
    type_filter["term"]["message_type"] = 0;
    filters.append(type_filter);

    root["query"]["bool"]["filter"] = filters;
    root["query"]["bool"]["must"]["match"]["content"] = keyword;
    return CompactJson(root);
}

void ConfiguredMessageSearchIndex::AddAuthHeader(
    const drogon::HttpRequestPtr &request) const {
    if (!user_.empty()) {
        request->addHeader("Authorization",
                           "Basic " + Base64Encode(user_ + ":" + password_));
    }
}

} // namespace zchat
