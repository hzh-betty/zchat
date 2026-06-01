#include "user/sms_client.h"

#include "user/user_errors.h"

namespace zchat {

VoidResult ConfiguredSmsClient::SendVerificationCode(const std::string &) {
    return VoidResult::Fail(user_errors::SmsClientNotConfigured());
}

VoidResult ConfiguredSmsClient::CheckVerificationCode(const std::string &,
                                                       const std::string &) {
    return VoidResult::Fail(user_errors::SmsClientNotConfigured());
}

} // namespace zchat
