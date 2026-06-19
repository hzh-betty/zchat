#include "common/search/message_search_index.h"

#include <sstream>
#include <utility>

#include <drogon/HttpRequest.h>
#include <nlohmann/json.hpp>

#include "common/crypto.h"
#include "common/logger.h"

namespace zchat {
using json = nlohmann::json;
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

std::string CompactJson(const json &value) { return value.dump(); }

MessageRecord MessageRecordFromJson(const json &source) {
    MessageRecord message;
    message.message_id = source.value("message_id", std::string());
    message.session_id = source.value("session_id", std::string());
    message.user_id = source.value("user_id", std::string());
    message.message_type = source.value("message_type", 0);
    message.create_time = source.value("create_time", std::int64_t(0));
    message.content = source.value("content", std::string());
    message.file_id = source.value("file_id", std::string());
    message.file_name = source.value("file_name", std::string());
    message.file_size = source.value("file_size", std::uint64_t(0));
    return message;
}

Result<std::vector<MessageRecord>>
ParseSearchResponse(const std::string &body) {
    json root;
    std::istringstream input(body);
    try {
        root = json::parse(input);
    } catch (const std::exception &e) {
        return Result<std::vector<MessageRecord>>::Fail(
            AppError::WithCode(
                ErrorCode::kExternalServiceError,
                "elasticsearch message search response parse failed")
                .WithDetail(e.what()));
    }

    std::vector<MessageRecord> messages;
    const json hits =
        root.value("hits", json::object()).value("hits", json::array());
    if (!hits.is_array()) {
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
    : host_(FirstHost(config.hosts)), user_(config.user),
      password_(config.password) {
    loop_thread_ =
        std::make_unique<trantor::EventLoopThread>("zchat-es-http-client");
    loop_thread_->run();
    client_ = drogon::HttpClient::newHttpClient(host_, loop_thread_->getLoop());
    const auto ensured = EnsureIndex();
    if (!ensured.ok()) {
        ZCHAT_LOG_WARN("Elasticsearch message index init failed: {}",
                       ensured.error().message);
    }
}

VoidResult ConfiguredMessageSearchIndex::EnsureIndex() {
    if (!client_) {
        return VoidResult::Fail(AppError::WithCode(
            ErrorCode::kExternalServiceError,
            "elasticsearch message client is not initialized"));
    }
    auto request = drogon::HttpRequest::newHttpRequest();
    request->setMethod(drogon::Put);
    request->setPath(kIndexPath);
    request->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    request->setBody(BuildElasticsearchMessageIndexDefinition());
    AddAuthHeader(request);

    const auto [result, response] =
        client_->sendRequest(request, kRequestTimeoutSeconds);
    if (result != drogon::ReqResult::Ok || !response) {
        return VoidResult::Fail(
            AppError::WithCode(
                ErrorCode::kExternalServiceError,
                "elasticsearch message index creation request failed")
                .WithDetail(drogon::to_string(result)));
    }
    if (IsSuccessStatus(response->statusCode())) {
        return VoidResult::Ok();
    }
    const std::string body(response->body());
    if (response->statusCode() == drogon::k400BadRequest &&
        body.find("resource_already_exists_exception") != std::string::npos) {
        return VoidResult::Ok();
    }
    return VoidResult::Fail(
        AppError::WithCode(ErrorCode::kExternalServiceError,
                           "elasticsearch message index creation failed")
            .WithContext("status", std::to_string(response->statusCode()))
            .WithDetail(body));
}

drogon::Task<VoidResult>
ConfiguredMessageSearchIndex::IndexMessageCoro(const MessageRecord &message) {
    if (!client_) {
        co_return VoidResult::Fail(AppError::WithCode(
            ErrorCode::kExternalServiceError,
            "elasticsearch message client is not initialized"));
    }

    auto request = drogon::HttpRequest::newHttpRequest();
    request->setMethod(drogon::Put);
    request->setPath(std::string(kIndexPath) + "/_doc/" + message.message_id);
    request->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    request->setBody(BuildElasticsearchMessageDocument(message));
    AddAuthHeader(request);

    auto response = co_await client_->sendRequestCoro(request, 3.0);
    if (!response) {
        co_return VoidResult::Fail(
            AppError::WithCode(ErrorCode::kExternalServiceError,
                               "elasticsearch message index request failed")
                .WithDetail("request failed"));
    }
    if (!IsSuccessStatus(response->statusCode())) {
        co_return VoidResult::Fail(
            AppError::WithCode(ErrorCode::kExternalServiceError,
                               "elasticsearch message index failed")
                .WithContext("status", std::to_string(response->statusCode())));
    }
    co_return VoidResult::Ok();
}

drogon::Task<Result<std::vector<MessageRecord>>>
ConfiguredMessageSearchIndex::SearchMessagesCoro(const std::string &session_id,
                                                 const std::string &keyword,
                                                 int offset, int limit) {
    if (!client_) {
        co_return Result<std::vector<MessageRecord>>::Fail(AppError::WithCode(
            ErrorCode::kExternalServiceError,
            "elasticsearch message client is not initialized"));
    }

    auto request = drogon::HttpRequest::newHttpRequest();
    request->setMethod(drogon::Post);
    request->setPath(std::string(kIndexPath) + "/_search");
    request->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    request->setBody(
        BuildElasticsearchSearchRequest(session_id, keyword, offset, limit));
    AddAuthHeader(request);

    auto response = co_await client_->sendRequestCoro(request, 3.0);
    if (!response) {
        co_return Result<std::vector<MessageRecord>>::Fail(
            AppError::WithCode(ErrorCode::kExternalServiceError,
                               "elasticsearch message search request failed")
                .WithDetail("request failed"));
    }
    if (!IsSuccessStatus(response->statusCode())) {
        co_return Result<std::vector<MessageRecord>>::Fail(
            AppError::WithCode(ErrorCode::kExternalServiceError,
                               "elasticsearch message search failed")
                .WithContext("status", std::to_string(response->statusCode())));
    }
    co_return ParseSearchResponse(std::string(response->body()));
}

std::string BuildElasticsearchMessageDocument(const MessageRecord &message) {
    json root(json::object());
    root["message_id"] = message.message_id;
    root["session_id"] = message.session_id;
    root["user_id"] = message.user_id;
    root["message_type"] = message.message_type;
    root["create_time"] = (message.create_time);
    root["content"] = message.content;
    root["file_id"] = message.file_id;
    root["file_name"] = message.file_name;
    root["file_size"] = (message.file_size);
    return CompactJson(root);
}

std::string BuildElasticsearchSearchRequest(const std::string &session_id,
                                            const std::string &keyword,
                                            int offset, int limit) {
    json root(json::object());
    root["from"] = offset;
    root["size"] = limit;
    root["sort"][0]["create_time"]["order"] = "asc";

    json filters(json::array());
    json session_filter(json::object());
    session_filter["term"]["session_id.keyword"] = session_id;
    filters.push_back(session_filter);
    json type_filter(json::object());
    type_filter["term"]["message_type"] = 0;
    filters.push_back(type_filter);

    root["query"]["bool"]["filter"] = filters;
    root["query"]["bool"]["must"]["match"]["content"] = keyword;
    return CompactJson(root);
}

std::string BuildElasticsearchMessageIndexDefinition() {
    json root(json::object());
    json properties(json::object());
    properties["message_id"]["type"] = "keyword";
    properties["session_id"]["type"] = "keyword";
    properties["user_id"]["type"] = "keyword";
    properties["message_type"]["type"] = "integer";
    properties["create_time"]["type"] = "long";
    properties["content"]["type"] = "text";
    properties["file_id"]["type"] = "keyword";
    properties["file_name"]["type"] = "keyword";
    properties["file_size"]["type"] = "long";
    root["mappings"]["properties"] = properties;
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
