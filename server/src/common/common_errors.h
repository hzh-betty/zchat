#ifndef ZCHAT_SERVER_SRC_COMMON_COMMON_ERRORS_H_
#define ZCHAT_SERVER_SRC_COMMON_COMMON_ERRORS_H_

#include "common/result.h"

namespace zchat::common_errors {

inline AppError RequestBodyParseFailed() {
    return AppError::WithCode(ErrorCode::kInvalidArgument,
                              "request body parse failed");
}

inline AppError SessionIdRequired() {
    return AppError::WithCode(ErrorCode::kUnauthorized,
                              "session id is required");
}

inline AppError SessionExpired() {
    return AppError::WithCode(ErrorCode::kSessionExpired, "session expired");
}

inline AppError UnknownServicePath() {
    return AppError::WithCode(ErrorCode::kNotFound, "unknown service path");
}

inline AppError ServiceEndpointNotFound() {
    return AppError::WithCode(ErrorCode::kExternalServiceError,
                              "service endpoint not found");
}

inline AppError RedisOperationFailed() {
    return AppError::WithCode(ErrorCode::kRedisError, "redis operation failed");
}

inline AppError DatabaseOperationFailed() {
    return AppError::WithCode(ErrorCode::kDatabaseError,
                              "database operation failed");
}

inline AppError InternalServiceError() {
    return AppError::WithCode(ErrorCode::kUnknown, "internal service error");
}

} // namespace zchat::common_errors

#endif // ZCHAT_SERVER_SRC_COMMON_COMMON_ERRORS_H_
