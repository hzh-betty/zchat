#include "common/search/user_search_index.h"

#include <sstream>
#include <utility>

#include <drogon/HttpRequest.h>
#include <nlohmann/json.hpp>

#include "common/crypto.h"
#include "common/logger.h"

namespace zchat {
using json = nlohmann::json;
namespace {

constexpr char kIndexPath[] = "/zchat_users";
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

bool IsSuccessStatus(drogon::HttpStatusCode status) {
    return status >= drogon::k200OK && status < drogon::k300MultipleChoices;
}

UserRecord UserRecordFromJson(const json &source) {
    UserRecord user;
    user.user_id = source.value("user_id", std::string());
    user.nickname = source.value("nickname", std::string());
    user.description = source.value("description", std::string());
    user.phone = source.value("phone", std::string());
    user.avatar_id = source.value("avatar_id", std::string());
    return user;
}

Result<std::vector<UserRecord>> ParseSearchResponse(const std::string &body) {
    json root;
    std::string errors;
    std::istringstream input(body);
    try {
        root = json::parse(input);
    } catch (const std::exception &e) {
        return Result<std::vector<UserRecord>>::Fail(
            AppError::WithCode(
                ErrorCode::kExternalServiceError,
                "elasticsearch user search response parse failed")
                .WithDetail(e.what()));
    }
    std::vector<UserRecord> users;
    const json hits =
        root.value("hits", json::object()).value("hits", json::array());
    if (!hits.is_array()) {
        return Result<std::vector<UserRecord>>::Ok(std::move(users));
    }
    for (const auto &hit : hits) {
        users.push_back(UserRecordFromJson(hit["_source"]));
    }
    return Result<std::vector<UserRecord>>::Ok(std::move(users));
}

} // namespace

ConfiguredUserSearchIndex::ConfiguredUserSearchIndex(
    const ElasticsearchConfig &config)
    : host_(FirstHost(config.hosts)), user_(config.user),
      password_(config.password) {
    loop_thread_ =
        std::make_unique<trantor::EventLoopThread>("zchat-es-user-client");
    loop_thread_->run();
    client_ = drogon::HttpClient::newHttpClient(host_, loop_thread_->getLoop());
    const auto ensured = EnsureIndex();
    if (!ensured.ok()) {
        ZCHAT_LOG_WARN("Elasticsearch user index init failed: {}",
                       ensured.error().message);
    }
}

VoidResult ConfiguredUserSearchIndex::EnsureIndex() {
    if (!client_) {
        return VoidResult::Fail(
            AppError::WithCode(ErrorCode::kExternalServiceError,
                               "elasticsearch user client is not initialized"));
    }
    auto request = drogon::HttpRequest::newHttpRequest();
    request->setMethod(drogon::Put);
    request->setPath(kIndexPath);
    request->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    request->setBody(BuildElasticsearchUserIndexDefinition());
    AddAuthHeader(request);

    const auto [result, response] =
        client_->sendRequest(request, kRequestTimeoutSeconds);
    if (!response) {
        return VoidResult::Fail(
            AppError::WithCode(
                ErrorCode::kExternalServiceError,
                "elasticsearch user index creation request failed")
                .WithDetail("request failed"));
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
                           "elasticsearch user index creation failed")
            .WithContext("status", std::to_string(response->statusCode()))
            .WithDetail(body));
}

drogon::Task<VoidResult>
ConfiguredUserSearchIndex::IndexUserCoro(const UserRecord &user) {
    if (!client_) {
        co_return VoidResult::Fail(
            AppError::WithCode(ErrorCode::kExternalServiceError,
                               "elasticsearch user client is not initialized"));
    }
    auto request = drogon::HttpRequest::newHttpRequest();
    request->setMethod(drogon::Put);
    request->setPath(std::string(kIndexPath) + "/_doc/" + user.user_id);
    request->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    request->setBody(BuildElasticsearchUserDocument(user));
    AddAuthHeader(request);

    auto response = co_await client_->sendRequestCoro(request);
    if (!response) {
        co_return VoidResult::Fail(
            AppError::WithCode(ErrorCode::kExternalServiceError,
                               "elasticsearch user index request failed")
                .WithDetail("request failed"));
    }
    if (!IsSuccessStatus(response->statusCode())) {
        co_return VoidResult::Fail(
            AppError::WithCode(ErrorCode::kExternalServiceError,
                               "elasticsearch user index failed")
                .WithContext("status", std::to_string(response->statusCode())));
    }
    co_return VoidResult::Ok();
}

drogon::Task<Result<std::vector<UserRecord>>>
ConfiguredUserSearchIndex::SearchUsersCoro(
    const std::string &keyword,
    const std::vector<std::string> &excluded_user_ids) {
    if (!client_) {
        co_return Result<std::vector<UserRecord>>::Fail(
            AppError::WithCode(ErrorCode::kExternalServiceError,
                               "elasticsearch user client is not initialized"));
    }
    auto request = drogon::HttpRequest::newHttpRequest();
    request->setMethod(drogon::Post);
    request->setPath(std::string(kIndexPath) + "/_search");
    request->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    request->setBody(
        BuildElasticsearchUserSearchRequest(keyword, excluded_user_ids));
    AddAuthHeader(request);

    auto response = co_await client_->sendRequestCoro(request);
    if (!response) {
        co_return Result<std::vector<UserRecord>>::Fail(
            AppError::WithCode(ErrorCode::kExternalServiceError,
                               "elasticsearch user search request failed")
                .WithDetail("request failed"));
    }
    if (!IsSuccessStatus(response->statusCode())) {
        co_return Result<std::vector<UserRecord>>::Fail(
            AppError::WithCode(ErrorCode::kExternalServiceError,
                               "elasticsearch user search failed")
                .WithContext("status", std::to_string(response->statusCode())));
    }
    co_return ParseSearchResponse(std::string(response->body()));
}

std::string BuildElasticsearchUserDocument(const UserRecord &user) {
    json root(json::object());
    root["user_id"] = user.user_id;
    root["nickname"] = user.nickname;
    root["description"] = user.description;
    root["phone"] = user.phone;
    root["avatar_id"] = user.avatar_id;
    return CompactJson(root);
}

std::string BuildElasticsearchUserSearchRequest(
    const std::string &keyword,
    const std::vector<std::string> &excluded_user_ids) {
    json root(json::object());
    root["size"] = 50;
    root["query"]["bool"]["should"][0]["term"]["user_id.keyword"] = keyword;
    root["query"]["bool"]["should"][1]["term"]["phone.keyword"] = keyword;
    root["query"]["bool"]["should"][2]["match"]["nickname"] = keyword;
    root["query"]["bool"]["minimum_should_match"] = 1;
    if (!excluded_user_ids.empty()) {
        for (const auto &id : excluded_user_ids) {
            root["query"]["bool"]["must_not"][0]["terms"]["user_id.keyword"]
                .push_back(id);
        }
    }
    return CompactJson(root);
}

std::string BuildElasticsearchUserIndexDefinition() {
    json root(json::object());
    json properties(json::object());
    properties["user_id"]["type"] = "keyword";
    properties["nickname"]["type"] = "text";
    properties["description"]["type"] = "text";
    properties["phone"]["type"] = "keyword";
    properties["avatar_id"]["type"] = "keyword";
    root["mappings"]["properties"] = properties;
    return CompactJson(root);
}

void ConfiguredUserSearchIndex::AddAuthHeader(
    const drogon::HttpRequestPtr &request) const {
    if (!user_.empty()) {
        request->addHeader("Authorization",
                           "Basic " + Base64Encode(user_ + ":" + password_));
    }
}

} // namespace zchat
