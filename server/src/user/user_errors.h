#ifndef ZCHAT_SERVER_SRC_USER_USER_ERRORS_H_
#define ZCHAT_SERVER_SRC_USER_USER_ERRORS_H_

#include "common/result.h"

namespace zchat::user_errors {

inline AppError InvalidPhone() {
    return AppError::WithCode(ErrorCode::kUserInvalidPhone, "手机号码格式错误");
}

inline AppError InvalidPassword() {
    return AppError::WithCode(ErrorCode::kUserInvalidPassword, "密码格式错误");
}

inline AppError UserAlreadyExists() {
    return AppError::WithCode(ErrorCode::kUserAlreadyExists, "用户已经存在");
}

inline AppError UserNotFound() {
    return AppError::WithCode(ErrorCode::kUserNotFound, "用户不存在");
}

inline AppError VerifyCodeInvalid() {
    return AppError::WithCode(ErrorCode::kUserVerifyCodeInvalid,
                              "验证码错误或已过期");
}

} // namespace zchat::user_errors

#endif // ZCHAT_SERVER_SRC_USER_USER_ERRORS_H_
