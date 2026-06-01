#ifndef ZCHAT_SERVER_SRC_USER_USER_ERRORS_H_
#define ZCHAT_SERVER_SRC_USER_USER_ERRORS_H_

#include "common/result.h"

namespace zchat::user_errors {

inline AppError NicknameRequired() {
    return AppError::WithCode(ErrorCode::kInvalidArgument,
                              "nickname is required");
}

inline AppError InvalidPhone() {
    return AppError::WithCode(ErrorCode::kUserInvalidPhone,
                              "invalid phone number");
}

inline AppError InvalidPassword() {
    return AppError::WithCode(ErrorCode::kUserInvalidPassword,
                              "invalid password format");
}

inline AppError NicknameAlreadyExists() {
    return AppError::WithCode(ErrorCode::kUserAlreadyExists,
                              "nickname already exists");
}

inline AppError PhoneAlreadyRegistered() {
    return AppError::WithCode(ErrorCode::kUserAlreadyExists,
                              "phone number already registered");
}

inline AppError UserNotFound() {
    return AppError::WithCode(ErrorCode::kUserNotFound, "user not found");
}

inline AppError VerifyCodeInvalid() {
    return AppError::WithCode(ErrorCode::kUserVerifyCodeInvalid,
                              "verification code is invalid or expired");
}

inline AppError VerifyCodeCheckFailed() {
    return AppError::WithCode(ErrorCode::kUserVerifyCodeInvalid,
                              "verification code check failed");
}

inline AppError VerifyCodeExpired() {
    return AppError::WithCode(ErrorCode::kUserVerifyCodeExpired,
                              "verification code expired");
}

inline AppError VerifyCodePhoneMismatch() {
    return AppError::WithCode(ErrorCode::kUserVerifyCodePhoneMismatch,
                              "verification code does not match phone number");
}

inline AppError InvalidCredentials() {
    return AppError::WithCode(ErrorCode::kUnauthorized,
                              "invalid nickname or password");
}

inline AppError PhoneNotRegistered() {
    return AppError::WithCode(ErrorCode::kUserNotFound,
                              "phone number is not registered");
}

inline AppError AlreadyLoggedIn() {
    return AppError::WithCode(ErrorCode::kUserAlreadyLoggedIn,
                              "user already logged in");
}

inline AppError SmsClientNotConfigured() {
    return AppError::WithCode(ErrorCode::kExternalServiceError,
                              "sms client is not configured");
}

} // namespace zchat::user_errors

#endif // ZCHAT_SERVER_SRC_USER_USER_ERRORS_H_
