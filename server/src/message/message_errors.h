#ifndef ZCHAT_SERVER_SRC_MESSAGE_MESSAGE_ERRORS_H_
#define ZCHAT_SERVER_SRC_MESSAGE_MESSAGE_ERRORS_H_

#include "common/result.h"

namespace zchat::message_errors {

inline AppError MessageNotFound() {
    return AppError::WithCode(ErrorCode::kMessageNotFound, "消息不存在");
}

} // namespace zchat::message_errors

#endif // ZCHAT_SERVER_SRC_MESSAGE_MESSAGE_ERRORS_H_
