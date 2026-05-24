#include "speech/baidu_speech_recognizer.h"

#include <json/json.h>

#include <sstream>

#include "common/crypto.h"
#include "common/logger.h"

namespace zchat {

BaiduSpeechRecognizer::BaiduSpeechRecognizer(const SpeechConfig &config)
    : app_id_(config.app_id), api_key_(config.api_key),
      secret_key_(config.secret_key) {
    loop_thread_ = std::make_unique<trantor::EventLoopThread>("zchat-baidu-asr");
    loop_thread_->run();
    client_ = drogon::HttpClient::newHttpClient("https://vop.baidu.com",
                                                loop_thread_->getLoop());
}

Result<std::string>
BaiduSpeechRecognizer::Recognize(const std::string &speech_data) {
    auto token_result = GetAccessToken();
    if (!token_result.ok()) {
        return Result<std::string>::Fail(token_result.error().message);
    }

    Json::Value body;
    body["format"] = "pcm";
    body["rate"] = 16000;
    body["channel"] = 1;
    body["cuid"] = app_id_.empty() ? std::string("zchat") : app_id_;
    body["token"] = token_result.value();
    body["dev_pid"] = 1537;
    body["speech"] = Base64Encode(speech_data);
    body["len"] = Json::Value::Int64(static_cast<Json::Int64>(speech_data.size()));

    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    const std::string body_str = Json::writeString(writer, body);

    auto request = drogon::HttpRequest::newHttpRequest();
    request->setMethod(drogon::Post);
    request->setPath("/server_api");
    request->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    request->setBody(body_str);

    const auto [result, response] = client_->sendRequest(request, 10.0);
    if (result != drogon::ReqResult::Ok || !response) {
        return Result<std::string>::Fail("百度语音识别请求失败: " +
                                         drogon::to_string(result));
    }
    if (response->statusCode() != 200) {
        return Result<std::string>::Fail(
            "百度语音识别返回异常状态: " +
            std::to_string(response->statusCode()));
    }

    Json::Value root;
    Json::CharReaderBuilder reader_builder;
    std::string errors;
    std::istringstream input(std::string(response->body()));
    if (!Json::parseFromStream(reader_builder, input, &root, &errors)) {
        return Result<std::string>::Fail("百度语音识别响应解析失败");
    }

    const int err_no = root["err_no"].asInt();
    if (err_no != 0) {
        std::string err_msg = root.get("err_msg", "unknown error").asString();
        return Result<std::string>::Fail("百度语音识别失败: " + err_msg);
    }

    const Json::Value &result_array = root["result"];
    if (!result_array.isArray() || result_array.empty()) {
        return Result<std::string>::Fail("百度语音识别返回空结果");
    }

    return Result<std::string>::Ok(result_array[0].asString());
}

Result<std::string> BaiduSpeechRecognizer::FetchAccessToken() {
    auto request = drogon::HttpRequest::newHttpRequest();
    request->setMethod(drogon::Post);
    request->setPath(std::string("/oauth/2.0/token?grant_type=client_credentials&client_id=") +
                     UrlEncode(api_key_) + "&client_secret=" + UrlEncode(secret_key_));

    auto token_client = drogon::HttpClient::newHttpClient(
        "https://aip.baidubce.com", loop_thread_->getLoop());
    const auto [result, response] = token_client->sendRequest(request, 10.0);
    if (result != drogon::ReqResult::Ok || !response) {
        return Result<std::string>::Fail(
            "百度 OAuth 令牌请求失败: " + drogon::to_string(result));
    }
    if (response->statusCode() != 200) {
        return Result<std::string>::Fail(
            "百度 OAuth 令牌返回异常状态: " +
            std::to_string(response->statusCode()));
    }

    Json::Value root;
    Json::CharReaderBuilder reader_builder;
    std::string errors;
    std::istringstream input(std::string(response->body()));
    if (!Json::parseFromStream(reader_builder, input, &root, &errors)) {
        return Result<std::string>::Fail("百度 OAuth 令牌响应解析失败");
    }

    if (root.isMember("error")) {
        return Result<std::string>::Fail(
            "百度 OAuth 令牌获取失败: " + root["error_description"].asString());
    }

    access_token_ = root["access_token"].asString();
    const int expires_in = root.get("expires_in", 2592000).asInt();
    token_expiry_ = std::chrono::steady_clock::now() +
                    std::chrono::seconds(expires_in - 600);

    return Result<std::string>::Ok(access_token_);
}

Result<std::string> BaiduSpeechRecognizer::GetAccessToken() {
    std::lock_guard<std::mutex> lock(token_mutex_);
    if (!access_token_.empty() &&
        std::chrono::steady_clock::now() < token_expiry_) {
        return Result<std::string>::Ok(access_token_);
    }
    return FetchAccessToken();
}

} // namespace zchat