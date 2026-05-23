#include "user/sms_client.h"

namespace zchat {

ConfiguredSmsClient::ConfiguredSmsClient(bool enabled) : enabled_(enabled) {}

VoidResult ConfiguredSmsClient::SendVerificationCode(const std::string &) {
    if (!enabled_) {
        return VoidResult::Ok();
    }
    return VoidResult::Fail(
        "阿里云短信认证 SDK 未接入，请填写配置并安装 SDK 后启用");
}

VoidResult ConfiguredSmsClient::CheckVerificationCode(const std::string &,
                                                       const std::string &) {
    if (!enabled_) {
        return VoidResult::Ok();
    }
    return VoidResult::Fail(
        "阿里云短信认证 SDK 未接入，请填写配置并安装 SDK 后启用");
}

} // namespace zchat