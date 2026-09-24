#ifndef ZCHAT_SERVER_SRC_COMMON_FILE_ERRORS_H_
#define ZCHAT_SERVER_SRC_COMMON_FILE_ERRORS_H_

#include "common/result.h"

namespace zchat {

// Preserve machine-readable prefixes of the existing protobuf error contract.
inline AppError FileUploadError(const std::string &message) {
    auto code = ErrorCode::kExternalServiceError;
    if (message.starts_with("FILE_STORAGE_REJECTED: ")) {
        code = ErrorCode::kFileStorageRejected;
    } else if (message.starts_with("RATE_LIMITED: ")) {
        code = ErrorCode::kRateLimited;
    }
    return AppError::WithCode(code, "file_service put file failed")
        .WithDetail(message);
}

} // namespace zchat

#endif
