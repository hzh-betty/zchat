#ifndef ZCHAT_SERVER_SRC_FRIEND_FRIEND_ERRORS_H_
#define ZCHAT_SERVER_SRC_FRIEND_FRIEND_ERRORS_H_

#include "common/result.h"

namespace zchat::friend_errors {

inline AppError AlreadyFriends() {
    return AppError::WithCode(ErrorCode::kFriendAlreadyExists,
                              "两者已经是好友关系");
}

inline AppError ApplyAlreadyExists() {
    return AppError::WithCode(ErrorCode::kFriendApplyAlreadyExists,
                              "已经申请过对方好友");
}

inline AppError SessionExpired() {
    return AppError::WithCode(ErrorCode::kUnauthorized, "登录会话已失效");
}

} // namespace zchat::friend_errors

#endif // ZCHAT_SERVER_SRC_FRIEND_FRIEND_ERRORS_H_
