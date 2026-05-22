#ifndef ZCHAT_SERVER_SRC_FILE_FILE_ERRORS_H_
#define ZCHAT_SERVER_SRC_FILE_FILE_ERRORS_H_

#include "common/result.h"

namespace zchat::file_errors {

inline AppError FileNotFound() {
    return AppError::WithCode(ErrorCode::kFileNotFound, "文件不存在");
}

} // namespace zchat::file_errors

#endif // ZCHAT_SERVER_SRC_FILE_FILE_ERRORS_H_
