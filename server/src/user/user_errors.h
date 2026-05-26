#ifndef ZCHAT_SERVER_SRC_USER_USER_ERRORS_H_
#define ZCHAT_SERVER_SRC_USER_USER_ERRORS_H_

#include "common/result.h"

namespace zchat::user_errors {

inline AppError InvalidPhone() {
    return AppError::WithCode(ErrorCode::kUserInvalidPhone,
                              "invalid phone number");
}

inline AppError InvalidPassword() {
    return AppError::WithCode(ErrorCode::kUserInvalidPassword,
                              "invalid password format");
}

inline AppError UserAlreadyExists() {
    return AppError::WithCode(ErrorCode::kUserAlreadyExists,
                              "user already exists");
}

inline AppError UserNotFound() {
    return AppError::WithCode(ErrorCode::kUserNotFound, "user not found");
}

inline AppError VerifyCodeInvalid() {
    return AppError::WithCode(ErrorCode::kUserVerifyCodeInvalid,
                              "verification code is invalid or expired");
}

} // namespace zchat::user_errors

#endif // ZCHAT_SERVER_SRC_USER_USER_ERRORS_H_
