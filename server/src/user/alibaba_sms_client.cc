#include "user/alibaba_sms_client.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#include <json/json.h>

#include "common/crypto.h"
#include "common/logger.h"
#include "common/uuid.h"

namespace zchat {

AlibabaSmsClient::AlibabaSmsClient(const SmsConfig &config)
    : access_key_id_(config.access_key_id),
      access_key_secret_(config.access_key_secret),
      sign_name_(config.sign_name),
      template_code_(config.template_code) {
    loop_thread_ = std::make_unique<trantor::EventLoopThread>("zchat-alibaba-sms");
    loop_thread_->run();
    client_ = drogon::HttpClient::newHttpClient("https://dysmsapi.aliyuncs.com",
                                                loop_thread_->getLoop());
}

VoidResult AlibabaSmsClient::SendVerificationCode(const std::string &phone) {
    std::map<std::string, std::string> params;
    params["Action"] = "SendVerificationCode";
    params["Format"] = "JSON";
    params["Version"] = "2017-05-25";
    params["AccessKeyId"] = access_key_id_;
    params["SignatureMethod"] = "HMAC-SHA1";
    params["SignatureVersion"] = "1.0";
    params["SignatureNonce"] = NewId();
    params["Timestamp"] = FormatUtcTimestamp();
    params["PhoneNumbers"] = phone;

    if (!sign_name_.empty()) {
        params["SignName"] = sign_name_;
    }
    if (!template_code_.empty()) {
        params["TemplateCode"] = template_code_;
    }

    return SendRequest(params);
}

VoidResult AlibabaSmsClient::CheckVerificationCode(const std::string &phone,
                                                   const std::string &code) {
    std::map<std::string, std::string> params;
    params["Action"] = "CheckVerificationCode";
    params["Format"] = "JSON";
    params["Version"] = "2017-05-25";
    params["AccessKeyId"] = access_key_id_;
    params["SignatureMethod"] = "HMAC-SHA1";
    params["SignatureVersion"] = "1.0";
    params["SignatureNonce"] = NewId();
    params["Timestamp"] = FormatUtcTimestamp();
    params["PhoneNumbers"] = phone;
    params["Code"] = code;

    return SendRequest(params);
}

VoidResult AlibabaSmsClient::SendRequest(
    const std::map<std::string, std::string> &params) {
    std::string signature = ComputeSignature(params, access_key_secret_);
    std::map<std::string, std::string> all_params = params;
    all_params["Signature"] = signature;

    std::string query = BuildQueryString(all_params);

    auto request = drogon::HttpRequest::newHttpRequest();
    request->setMethod(drogon::Get);
    request->setPath("/?" + query);

    const auto [result, response] = client_->sendRequest(request, 10.0);
    if (result != drogon::ReqResult::Ok || !response) {
        return VoidResult::Fail("阿里云短信请求失败: " +
                                drogon::to_string(result));
    }
    if (response->statusCode() != 200) {
        return VoidResult::Fail("阿里云短信返回异常状态: " +
                                std::to_string(response->statusCode()));
    }

    Json::Value root;
    Json::CharReaderBuilder reader_builder;
    std::string errors;
    std::istringstream input(std::string(response->body()));
    if (!Json::parseFromStream(reader_builder, input, &root, &errors)) {
        return VoidResult::Fail("阿里云短信响应解析失败");
    }

    std::string code_resp = root.get("Code", "").asString();
    if (code_resp != "OK") {
        std::string message = root.get("Message", "unknown error").asString();
        return VoidResult::Fail("阿里云短信操作失败: " + message);
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
    return UrlEncode(HmacSha1(key, string_to_sign));
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