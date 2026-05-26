#ifndef ZCHAT_SERVER_SRC_COMMON_ERROR_RESPONSE_H_
#define ZCHAT_SERVER_SRC_COMMON_ERROR_RESPONSE_H_

#include <string>

#include "common/logger.h"
#include "common/result.h"

namespace zchat {

template <typename Response>
Response MakeErrorResponse(const std::string &request_id,
                           const AppError &error) {
    Response response;
    response.set_request_id(request_id);
    response.set_success(false);
    response.set_errmsg(FormatErrorForClient(error));
    return response;
}

template <typename Response>
Response MakeErrorResponse(const std::string &request_id, ErrorCode code,
                           const std::string &message) {
    return MakeErrorResponse<Response>(request_id,
                                       AppError::WithCode(code, message));
}

template <typename Response>
Response MakeErrorResponse(const std::string &request_id,
                           const std::string &message) {
    return MakeErrorResponse<Response>(
        request_id, AppError::WithCode(ErrorCode::kUnknown, message));
}

inline void LogBoundaryError(const char *service, const char *method,
                             const std::string &request_id,
                             const std::string &errmsg) {
    ZCHAT_LOG_WARN("{}::{} failed request_id={} error={}", service, method,
                   request_id, errmsg);
}

template <typename Response>
void LogBoundaryResponseError(const char *service, const char *method,
                              const std::string &request_id,
                              const Response &response) {
    if (!response.success()) {
        LogBoundaryError(service, method, request_id, response.errmsg());
    }
}

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_COMMON_ERROR_RESPONSE_H_
