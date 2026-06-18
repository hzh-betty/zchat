#include "user/sms_client.h"

#include "user/user_errors.h"

namespace zchat {

drogon::Task<VoidResult>
ConfiguredSmsClient::SendVerificationCode(const std::string &) {
    co_return VoidResult::Fail(user_errors::SmsClientNotConfigured());
}

drogon::Task<VoidResult>
ConfiguredSmsClient::CheckVerificationCode(const std::string &,
                                           const std::string &) {
    co_return VoidResult::Fail(user_errors::SmsClientNotConfigured());
}

} // namespace zchat
