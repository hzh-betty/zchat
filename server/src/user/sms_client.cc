#include "user/sms_client.h"

namespace zchat {

VoidResult ConfiguredSmsClient::SendVerificationCode(const std::string &) {
    return VoidResult::Fail(AppError::WithCode(
        ErrorCode::kExternalServiceError, "sms client is not configured"));
}

VoidResult ConfiguredSmsClient::CheckVerificationCode(const std::string &,
                                                       const std::string &) {
    return VoidResult::Fail(AppError::WithCode(
        ErrorCode::kExternalServiceError, "sms client is not configured"));
}

} // namespace zchat
