#ifndef ZCHAT_SERVER_SRC_TRANSMITE_TRANSMITE_ERRORS_H_
#define ZCHAT_SERVER_SRC_TRANSMITE_TRANSMITE_ERRORS_H_

#include "common/result.h"

namespace zchat::transmite_errors {

inline AppError TargetNotFound() {
    return AppError::WithCode(ErrorCode::kTransmitTargetNotFound,
                              "message transmit target not found");
}

} // namespace zchat::transmite_errors

#endif // ZCHAT_SERVER_SRC_TRANSMITE_TRANSMITE_ERRORS_H_
