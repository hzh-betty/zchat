#include "user/sms_client.h"

namespace zchat {

ConfiguredSmsClient::ConfiguredSmsClient(bool enabled) : enabled_(enabled) {}

VoidResult ConfiguredSmsClient::SendVerifyCode(const std::string &,
                                               const std::string &) {
    if (!enabled_) {
        return VoidResult::Ok();
    }
    return VoidResult::Fail(
        "阿里云短信 SDK 未接入，请填写配置并安装 SDK 后启用");
}

} // namespace zchat
