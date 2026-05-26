#include "user/user_search_index.h"

#include <sstream>
#include <utility>

#include <drogon/HttpRequest.h>
#include <json/json.h>

#include "common/crypto.h"
#include "common/logger.h"

namespace zchat {
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

std::string CompactJson(const Json::Value &value) {
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, value);
}

bool IsSuccessStatus(drogon::HttpStatusCode status) {
    return status >= drogon::k200OK && status < drogon::k300MultipleChoices;
}

UserRecord UserRecordFromJson(const Json::Value &source) {
    UserRecord user;
    user.user_id = source["user_id"].asString();
    user.nickname = source["nickname"].asString();
    user.description = source["description"].asString();
    user.phone = source["phone"].asString();
    user.avatar_id = source["avatar_id"].asString();
    return user;
}

Result<std::vector<UserRecord>> ParseSearchResponse(const std::string &body) {
    Json::CharReaderBuilder builder;
    Json::Value root;
    std::string errors;
    std::istringstream input(body);
    if (!Json::parseFromStream(builder, input, &root, &errors)) {
        return Result<std::vector<UserRecord>>::Fail(
            "Elasticsearch 用户搜索响应解析失败: " + errors);
    }
    std::vector<UserRecord> users;
    const Json::Value hits = root["hits"]["hits"];
    if (!hits.isArray()) {
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
    : enabled_(config.enabled), host_(FirstHost(config.hosts)),
      user_(config.user), password_(config.password) {
    if (!enabled_) {
        return;
    }
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
    if (!enabled_) {
        return VoidResult::Ok();
    }
    if (!client_) {
        return VoidResult::Fail("Elasticsearch 用户客户端未初始化");
    }
    auto request = drogon::HttpRequest::newHttpRequest();
    request->setMethod(drogon::Put);
    request->setPath(kIndexPath);
    request->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    request->setBody(BuildElasticsearchUserIndexDefinition());
    AddAuthHeader(request);

    const auto [result, response] =
        client_->sendRequest(request, kRequestTimeoutSeconds);
    if (result != drogon::ReqResult::Ok || !response) {
        return VoidResult::Fail("Elasticsearch 用户索引创建请求失败: " +
                                drogon::to_string(result));
    }
    if (IsSuccessStatus(response->statusCode())) {
        return VoidResult::Ok();
    }
    const std::string body(response->body());
    if (response->statusCode() == drogon::k400BadRequest &&
        body.find("resource_already_exists_exception") != std::string::npos) {
        return VoidResult::Ok();
    }
    return VoidResult::Fail("Elasticsearch 用户索引创建返回异常状态: " +
                            std::to_string(response->statusCode()) +
                            " body=" + body);
}

VoidResult ConfiguredUserSearchIndex::IndexUser(const UserRecord &user) {
    if (!enabled_) {
        return VoidResult::Ok();
    }
    if (!client_) {
        return VoidResult::Fail("Elasticsearch 用户客户端未初始化");
    }
    auto request = drogon::HttpRequest::newHttpRequest();
    request->setMethod(drogon::Put);
    request->setPath(std::string(kIndexPath) + "/_doc/" + user.user_id);
    request->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    request->setBody(BuildElasticsearchUserDocument(user));
    AddAuthHeader(request);

    const auto [result, response] =
        client_->sendRequest(request, kRequestTimeoutSeconds);
    if (result != drogon::ReqResult::Ok || !response) {
        return VoidResult::Fail("Elasticsearch 用户索引请求失败: " +
                                drogon::to_string(result));
    }
    if (!IsSuccessStatus(response->statusCode())) {
        return VoidResult::Fail("Elasticsearch 用户索引返回异常状态: " +
                                std::to_string(response->statusCode()));
    }
    return VoidResult::Ok();
}

Result<std::vector<UserRecord>> ConfiguredUserSearchIndex::SearchUsers(
    const std::string &keyword,
    const std::vector<std::string> &excluded_user_ids) {
    if (!enabled_) {
        return Result<std::vector<UserRecord>>::Fail("Elasticsearch 未启用");
    }
    if (!client_) {
        return Result<std::vector<UserRecord>>::Fail(
            "Elasticsearch 用户客户端未初始化");
    }
    auto request = drogon::HttpRequest::newHttpRequest();
    request->setMethod(drogon::Post);
    request->setPath(std::string(kIndexPath) + "/_search");
    request->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    request->setBody(
        BuildElasticsearchUserSearchRequest(keyword, excluded_user_ids));
    AddAuthHeader(request);

    const auto [result, response] =
        client_->sendRequest(request, kRequestTimeoutSeconds);
    if (result != drogon::ReqResult::Ok || !response) {
        return Result<std::vector<UserRecord>>::Fail(
            "Elasticsearch 用户搜索请求失败: " + drogon::to_string(result));
    }
    if (!IsSuccessStatus(response->statusCode())) {
        return Result<std::vector<UserRecord>>::Fail(
            "Elasticsearch 用户搜索返回异常状态: " +
            std::to_string(response->statusCode()));
    }
    return ParseSearchResponse(std::string(response->body()));
}

std::string BuildElasticsearchUserDocument(const UserRecord &user) {
    Json::Value root(Json::objectValue);
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
    Json::Value root(Json::objectValue);
    root["size"] = 50;
    root["query"]["bool"]["should"][0]["term"]["user_id.keyword"] = keyword;
    root["query"]["bool"]["should"][1]["term"]["phone.keyword"] = keyword;
    root["query"]["bool"]["should"][2]["match"]["nickname"] = keyword;
    root["query"]["bool"]["minimum_should_match"] = 1;
    if (!excluded_user_ids.empty()) {
        for (const auto &id : excluded_user_ids) {
            root["query"]["bool"]["must_not"][0]["terms"]["user_id.keyword"]
                .append(id);
        }
    }
    return CompactJson(root);
}

std::string BuildElasticsearchUserIndexDefinition() {
    Json::Value root(Json::objectValue);
    Json::Value properties(Json::objectValue);
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
