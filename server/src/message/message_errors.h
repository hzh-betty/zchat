#ifndef ZCHAT_SERVER_SRC_MESSAGE_MESSAGE_ERRORS_H_
#define ZCHAT_SERVER_SRC_MESSAGE_MESSAGE_ERRORS_H_

#include "common/result.h"

namespace zchat::message_errors {

inline AppError MessageNotFound() {
    return AppError::WithCode(ErrorCode::kMessageNotFound,
                              "message not found");
}

inline AppError SessionAccessDenied() {
    return AppError::WithCode(ErrorCode::kForbidden,
                              "chat session access denied");
}

inline AppError QueuePayloadParseFailed() {
    return AppError::WithCode(ErrorCode::kInvalidArgument,
                              "rabbitmq message payload parse failed");
}

} // namespace zchat::message_errors

#endif // ZCHAT_SERVER_SRC_MESSAGE_MESSAGE_ERRORS_H_
