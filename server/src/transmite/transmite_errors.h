#ifndef ZCHAT_SERVER_SRC_TRANSMITE_TRANSMITE_ERRORS_H_
#define ZCHAT_SERVER_SRC_TRANSMITE_TRANSMITE_ERRORS_H_

#include "common/result.h"

namespace zchat::transmite_errors {

inline AppError TargetNotFound() {
    return AppError::WithCode(ErrorCode::kTransmitTargetNotFound,
                              "消息转发目标不存在");
}

} // namespace zchat::transmite_errors

#endif // ZCHAT_SERVER_SRC_TRANSMITE_TRANSMITE_ERRORS_H_
