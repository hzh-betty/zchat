#include "speech/baidu_speech_recognizer.h"

#include <nlohmann/json.hpp>

#include <sstream>

#include "common/crypto.h"
#include "common/logger.h"
#include "common/result.h"
#include "speech/speech_errors.h"

namespace zchat {
using json = nlohmann::json;

BaiduSpeechRecognizer::BaiduSpeechRecognizer(const SpeechConfig &config)
    : app_id_(config.app_id), api_key_(config.api_key),
      secret_key_(config.secret_key) {
    loop_thread_ =
        std::make_unique<trantor::EventLoopThread>("zchat-baidu-asr");
    loop_thread_->run();
    client_ = drogon::HttpClient::newHttpClient("https://vop.baidu.com",
                                                loop_thread_->getLoop());
}

drogon::Task<Result<std::string>>
BaiduSpeechRecognizer::RecognizeCoro(const std::string &speech_data) {
    auto token_result = co_await GetAccessTokenCoro();
    if (!token_result.ok()) {
        co_return Result<std::string>::Fail(token_result.error());
    }

    json body;
    body["format"] = "pcm";
    body["rate"] = 16000;
    body["channel"] = 1;
    body["cuid"] = app_id_.empty() ? std::string("zchat") : app_id_;
    body["token"] = token_result.value();
    body["dev_pid"] = 1537;
    body["speech"] = Base64Encode(speech_data);
    body["len"] = (static_cast<std::int64_t>(speech_data.size()));

    const std::string body_str = body.dump();

    auto request = drogon::HttpRequest::newHttpRequest();
    request->setMethod(drogon::Post);
    request->setPath("/server_api");
    request->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    request->setBody(body_str);

    auto response = co_await client_->sendRequestCoro(request);
    if (!response) {
        co_return Result<std::string>::Fail(
            speech_errors::RecognitionFailed(
                "baidu speech recognition request failed")
                .WithDetail("request failed"));
    }
    if (response->statusCode() != 200) {
        co_return Result<std::string>::Fail(
            speech_errors::RecognitionFailed(
                "baidu speech recognition request failed")
                .WithContext("status", std::to_string(response->statusCode())));
    }

    json root;
    std::string errors;
    std::istringstream input(std::string(response->body()));
    try {
        root = json::parse(input);
    } catch (const std::exception &e) {
        co_return Result<std::string>::Fail(
            speech_errors::RecognitionFailed(
                "baidu speech recognition response parse failed")
                .WithDetail(e.what()));
    }

    const int err_no = root.value("err_no", 0);
    if (err_no != 0) {
        std::string err_msg =
            root.value("err_msg", std::string("unknown error"));
        co_return Result<std::string>::Fail(
            speech_errors::RecognitionFailed("baidu speech recognition failed")
                .WithContext("provider_code", std::to_string(err_no))
                .WithDetail(err_msg));
    }

    const json result_array = root.value("result", json::array());
    if (!result_array.is_array() || result_array.empty()) {
        co_return Result<std::string>::Fail(speech_errors::RecognitionFailed(
            "baidu speech recognition returned empty result"));
    }

    co_return Result<std::string>::Ok(result_array[0]);
}

drogon::Task<Result<std::string>>
BaiduSpeechRecognizer::FetchAccessTokenCoro() {
    auto request = drogon::HttpRequest::newHttpRequest();
    request->setMethod(drogon::Post);
    request->setPath(
        std::string(
            "/oauth/2.0/token?grant_type=client_credentials&client_id=") +
        UrlEncode(api_key_) + "&client_secret=" + UrlEncode(secret_key_));

    auto token_client = drogon::HttpClient::newHttpClient(
        "https://aip.baidubce.com", loop_thread_->getLoop());
    auto response = co_await token_client->sendRequestCoro(request);
    if (!response) {
        co_return Result<std::string>::Fail(
            AppError::WithCode(ErrorCode::kExternalServiceError,
                               "baidu oauth token request failed")
                .WithDetail("request failed"));
    }
    if (response->statusCode() != 200) {
        co_return Result<std::string>::Fail(
            AppError::WithCode(ErrorCode::kExternalServiceError,
                               "baidu oauth token request failed")
                .WithContext("status", std::to_string(response->statusCode())));
    }

    json root;
    std::string errors;
    std::istringstream input(std::string(response->body()));
    try {
        root = json::parse(input);
    } catch (const std::exception &e) {
        co_return Result<std::string>::Fail(
            AppError::WithCode(ErrorCode::kExternalServiceError,
                               "baidu oauth token response parse failed")
                .WithDetail(e.what()));
    }

    if (root.contains("error")) {
        co_return Result<std::string>::Fail(
            AppError::WithCode(ErrorCode::kExternalServiceError,
                               "baidu oauth token request failed")
                .WithDetail(root.value("error_description", std::string())));
    }

    access_token_ = root.value("access_token", std::string());
    const int expires_in = root.value("expires_in", 2592000);
    token_expiry_ = std::chrono::steady_clock::now() +
                    std::chrono::seconds(expires_in - 600);

    co_return Result<std::string>::Ok(access_token_);
}

drogon::Task<Result<std::string>> BaiduSpeechRecognizer::GetAccessTokenCoro() {
    std::lock_guard<std::mutex> lock(token_mutex_);
    if (!access_token_.empty() &&
        std::chrono::steady_clock::now() < token_expiry_) {
        co_return Result<std::string>::Ok(access_token_);
    }
    co_return co_await FetchAccessTokenCoro();
}

} // namespace zchat
