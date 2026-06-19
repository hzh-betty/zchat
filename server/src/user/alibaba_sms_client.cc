#include "user/alibaba_sms_client.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#include <nlohmann/json.hpp>

#include "common/crypto.h"
#include "common/logger.h"
#include "common/result.h"
#include "common/uuid.h"
#include "user/user_errors.h"

namespace zchat {
using json = nlohmann::json;
namespace {

constexpr char kDypnsEndpoint[] = "https://dypnsapi.aliyuncs.com";
constexpr char kApiVersion[] = "2017-05-25";
constexpr char kTemplateParam[] = R"({"code":"##code##","min":"5"})";
constexpr char kCodeTypeNumber[] = "1";

} // namespace

AlibabaSmsClient::AlibabaSmsClient(const SmsConfig &config)
    : access_key_id_(config.access_key_id),
      access_key_secret_(config.access_key_secret),
      sign_name_(config.sign_name), template_code_(config.template_code) {
    loop_thread_ =
        std::make_unique<trantor::EventLoopThread>("zchat-alibaba-sms");
    loop_thread_->run();
    client_ = drogon::HttpClient::newHttpClient(kDypnsEndpoint,
                                                loop_thread_->getLoop());
}

drogon::Task<VoidResult>
AlibabaSmsClient::SendVerificationCode(const std::string &phone) {
    std::string config_error;
    if (!HasRequiredConfig(&config_error)) {
        co_return VoidResult::Fail(
            AppError::WithCode(ErrorCode::kExternalServiceError, config_error));
    }
    std::map<std::string, std::string> params;
    params["Action"] = "SendSmsVerifyCode";
    params["Format"] = "JSON";
    params["Version"] = kApiVersion;
    params["AccessKeyId"] = access_key_id_;
    params["SignatureMethod"] = "HMAC-SHA1";
    params["SignatureVersion"] = "1.0";
    params["SignatureNonce"] = NewId();
    params["Timestamp"] = FormatUtcTimestamp();
    params["PhoneNumber"] = phone;
    params["SignName"] = sign_name_;
    params["TemplateCode"] = template_code_;
    params["TemplateParam"] = kTemplateParam;
    params["CodeType"] = kCodeTypeNumber;

    co_return co_await SendRequestCoro(params);
}

drogon::Task<VoidResult>
AlibabaSmsClient::CheckVerificationCode(const std::string &phone,
                                        const std::string &code) {
    std::string config_error;
    if (!HasRequiredConfig(&config_error)) {
        co_return VoidResult::Fail(
            AppError::WithCode(ErrorCode::kExternalServiceError, config_error));
    }
    std::map<std::string, std::string> params;
    params["Action"] = "CheckSmsVerifyCode";
    params["Format"] = "JSON";
    params["Version"] = kApiVersion;
    params["AccessKeyId"] = access_key_id_;
    params["SignatureMethod"] = "HMAC-SHA1";
    params["SignatureVersion"] = "1.0";
    params["SignatureNonce"] = NewId();
    params["Timestamp"] = FormatUtcTimestamp();
    params["PhoneNumber"] = phone;
    params["VerifyCode"] = code;

    co_return co_await SendRequestCoro(params, true);
}

bool AlibabaSmsClient::HasRequiredConfig(std::string *message) const {
    if (access_key_id_.empty() || access_key_secret_.empty() ||
        sign_name_.empty() || template_code_.empty()) {
        if (message != nullptr) {
            *message = "alibaba sms configuration is incomplete";
        }
        return false;
    }
    return true;
}

drogon::Task<VoidResult> AlibabaSmsClient::SendRequestCoro(
    const std::map<std::string, std::string> &params,
    bool check_verify_result) {
    std::string signature = ComputeSignature(params, access_key_secret_);
    std::map<std::string, std::string> all_params = params;
    all_params["Signature"] = signature;

    std::string query = BuildQueryString(all_params);

    auto request = drogon::HttpRequest::newHttpRequest();
    request->setMethod(drogon::Get);
    request->setPath("/?" + query);

    auto response = co_await client_->sendRequestCoro(request, 5.0);
    if (!response) {
        co_return VoidResult::Fail(
            AppError::WithCode(ErrorCode::kExternalServiceError,
                               "alibaba sms request failed")
                .WithDetail("request failed"));
    }
    json root;
    std::string errors;
    std::istringstream input(std::string(response->body()));
    try {
        root = json::parse(input);
    } catch (const std::exception &e) {
        if (response->statusCode() != 200) {
            co_return VoidResult::Fail(
                AppError::WithCode(ErrorCode::kExternalServiceError,
                                   "alibaba sms request failed")
                    .WithContext("status",
                                 std::to_string(response->statusCode()))
                    .WithDetail(std::string(response->body())));
        }
        co_return VoidResult::Fail(
            AppError::WithCode(ErrorCode::kExternalServiceError,
                               "alibaba sms response parse failed")
                .WithDetail(e.what()));
    }

    if (response->statusCode() != 200) {
        std::string code_resp =
            root.value("Code", root.value("code", std::string()));
        std::string message = root.value(
            "Message", root.value("message", std::string("unknown error")));
        co_return VoidResult::Fail(
            AppError::WithCode(ErrorCode::kExternalServiceError,
                               "alibaba sms request failed")
                .WithContext("status", std::to_string(response->statusCode()))
                .WithContext("provider_code", code_resp)
                .WithDetail(message));
    }

    std::string code_resp =
        root.value("Code", root.value("code", std::string()));
    if (code_resp != "OK") {
        std::string message = root.value(
            "Message", root.value("message", std::string("unknown error")));
        if (code_resp.empty() && message == "unknown error") {
            co_return VoidResult::Fail(
                AppError::WithCode(ErrorCode::kExternalServiceError,
                                   "alibaba sms operation failed")
                    .WithDetail(std::string(response->body())));
        }
        co_return VoidResult::Fail(
            AppError::WithCode(ErrorCode::kExternalServiceError,
                               "alibaba sms operation failed")
                .WithContext("provider_code", code_resp)
                .WithDetail(message +
                            " body=" + std::string(response->body())));
    }

    if (check_verify_result) {
        const json &model = root.contains("Model")
                                ? root.value("Model", json::object())
                                : root.value("model", json::object());
        if (model.is_object()) {
            std::string verify_result = model.value(
                "VerifyResult", model.value("verifyResult", std::string()));
            if (verify_result != "PASS") {
                co_return VoidResult::Fail(
                    user_errors::VerifyCodeCheckFailed());
            }
        } else {
            co_return VoidResult::Fail(AppError::WithCode(
                ErrorCode::kExternalServiceError,
                "alibaba sms check response is missing model"));
        }
    }

    co_return VoidResult::Ok();
}

std::string AlibabaSmsClient::FormatUtcTimestamp() const {
    auto now = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_utc{};
    gmtime_r(&time, &tm_utc);
    std::ostringstream oss;
    oss << std::put_time(&tm_utc, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

std::string AlibabaSmsClient::ComputeSignature(
    const std::map<std::string, std::string> &params,
    const std::string &secret) const {
    std::ostringstream canonical;
    bool first = true;
    for (const auto &[key, value] : params) {
        if (!first)
            canonical << "&";
        canonical << UrlEncode(key) << "=" << UrlEncode(value);
        first = false;
    }

    std::string string_to_sign =
        "GET&" + UrlEncode("/") + "&" + UrlEncode(canonical.str());
    std::string key = secret + "&";
    return HmacSha1(key, string_to_sign);
}

std::string AlibabaSmsClient::BuildQueryString(
    const std::map<std::string, std::string> &params) const {
    std::ostringstream oss;
    bool first = true;
    for (const auto &[key, value] : params) {
        if (!first)
            oss << "&";
        oss << UrlEncode(key) << "=" << UrlEncode(value);
        first = false;
    }
    return oss.str();
}

} // namespace zchat
