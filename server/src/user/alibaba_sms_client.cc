#include "user/alibaba_sms_client.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#include <json/json.h>

#include "common/crypto.h"
#include "common/logger.h"
#include "common/result.h"
#include "common/uuid.h"

namespace zchat {
namespace {

constexpr char kDypnsEndpoint[] = "https://dypnsapi.aliyuncs.com";
constexpr char kApiVersion[] = "2017-05-25";
constexpr char kTemplateParam[] = R"({"code":"##code##","min":"5"})";
constexpr char kCodeTypeNumber[] = "1";

} // namespace

AlibabaSmsClient::AlibabaSmsClient(const SmsConfig &config)
    : access_key_id_(config.access_key_id),
      access_key_secret_(config.access_key_secret),
      sign_name_(config.sign_name),
      template_code_(config.template_code) {
    loop_thread_ = std::make_unique<trantor::EventLoopThread>("zchat-alibaba-sms");
    loop_thread_->run();
    client_ =
        drogon::HttpClient::newHttpClient(kDypnsEndpoint, loop_thread_->getLoop());
}

VoidResult AlibabaSmsClient::SendVerificationCode(const std::string &phone) {
    std::string config_error;
    if (!HasRequiredConfig(&config_error)) {
        return VoidResult::Fail(AppError::WithCode(
            ErrorCode::kExternalServiceError, config_error));
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

    return SendRequest(params);
}

VoidResult AlibabaSmsClient::CheckVerificationCode(const std::string &phone,
                                                   const std::string &code) {
    std::string config_error;
    if (!HasRequiredConfig(&config_error)) {
        return VoidResult::Fail(AppError::WithCode(
            ErrorCode::kExternalServiceError, config_error));
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

    return SendRequest(params, true);
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

VoidResult AlibabaSmsClient::SendRequest(
    const std::map<std::string, std::string> &params, bool check_verify_result) {
    std::string signature = ComputeSignature(params, access_key_secret_);
    std::map<std::string, std::string> all_params = params;
    all_params["Signature"] = signature;

    std::string query = BuildQueryString(all_params);

    auto request = drogon::HttpRequest::newHttpRequest();
    request->setMethod(drogon::Get);
    request->setPathEncode(false);
    request->setPath("/?" + query);

    const auto [result, response] = client_->sendRequest(request, 10.0);
    if (result != drogon::ReqResult::Ok || !response) {
        return VoidResult::Fail(
            AppError::WithCode(ErrorCode::kExternalServiceError,
                               "alibaba sms request failed")
                .WithDetail(drogon::to_string(result)));
    }
    Json::Value root;
    Json::CharReaderBuilder reader_builder;
    std::string errors;
    std::istringstream input(std::string(response->body()));
    if (!Json::parseFromStream(reader_builder, input, &root, &errors)) {
        if (response->statusCode() != 200) {
            return VoidResult::Fail(
                AppError::WithCode(ErrorCode::kExternalServiceError,
                                   "alibaba sms request failed")
                    .WithContext("status",
                                 std::to_string(response->statusCode()))
                    .WithDetail(std::string(response->body())));
        }
        return VoidResult::Fail(
            AppError::WithCode(ErrorCode::kExternalServiceError,
                               "alibaba sms response parse failed")
                .WithDetail(errors));
    }

    if (response->statusCode() != 200) {
        std::string code_resp = root.get("Code", root.get("code", "")).asString();
        std::string message =
            root.get("Message", root.get("message", "unknown error")).asString();
        return VoidResult::Fail(
            AppError::WithCode(ErrorCode::kExternalServiceError,
                               "alibaba sms request failed")
                .WithContext("status", std::to_string(response->statusCode()))
                .WithContext("provider_code", code_resp)
                .WithDetail(message));
    }

    std::string code_resp = root.get("Code", root.get("code", "")).asString();
    if (code_resp != "OK") {
        std::string message =
            root.get("Message", root.get("message", "unknown error")).asString();
        if (code_resp.empty() && message == "unknown error") {
            return VoidResult::Fail(
                AppError::WithCode(ErrorCode::kExternalServiceError,
                                   "alibaba sms operation failed")
                    .WithDetail(std::string(response->body())));
        }
        return VoidResult::Fail(
            AppError::WithCode(ErrorCode::kExternalServiceError,
                               "alibaba sms operation failed")
                .WithContext("provider_code", code_resp)
                .WithDetail(message + " body=" + std::string(response->body())));
    }

    if (check_verify_result) {
        const Json::Value &model =
            root.isMember("Model") ? root["Model"] : root["model"];
        if (model.isObject()) {
            std::string verify_result =
                model.get("VerifyResult", model.get("verifyResult", "")).asString();
            if (verify_result != "PASS") {
                return VoidResult::Fail(AppError::WithCode(
                    ErrorCode::kUserVerifyCodeInvalid,
                    "verification code check failed"));
            }
        } else {
            return VoidResult::Fail(
                AppError::WithCode(ErrorCode::kExternalServiceError,
                                   "alibaba sms check response is missing model"));
        }
    }

    return VoidResult::Ok();
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
