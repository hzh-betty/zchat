#ifndef ZCHAT_SERVER_SRC_FRIEND_FRIEND_ERRORS_H_
#define ZCHAT_SERVER_SRC_FRIEND_FRIEND_ERRORS_H_

#include "common/result.h"

namespace zchat::friend_errors {

inline AppError AlreadyFriends() {
    return AppError::WithCode(ErrorCode::kFriendAlreadyExists,
                              "friend relation already exists");
}

inline AppError ApplyAlreadyExists() {
    return AppError::WithCode(ErrorCode::kFriendApplyAlreadyExists,
                              "friend request already exists");
}

} // namespace zchat::friend_errors

#endif // ZCHAT_SERVER_SRC_FRIEND_FRIEND_ERRORS_H_
