#ifndef ZCHAT_SERVER_SRC_COMMON_RESULT_H_
#define ZCHAT_SERVER_SRC_COMMON_RESULT_H_

#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace zchat {

enum class ErrorCode {
    kUnknown = 0,
    kInvalidArgument = 1,
    kUnauthorized = 2,
    kForbidden = 3,
    kNotFound = 4,
    kConflict = 5,
    kDatabaseError = 6,
    kRedisError = 7,
    kExternalServiceError = 8,
    kTimeout = 9,
    kSessionExpired = 10,

    kUserInvalidPhone = 1001,
    kUserInvalidPassword = 1002,
    kUserAlreadyExists = 1003,
    kUserNotFound = 1004,
    kUserVerifyCodeInvalid = 1005,
    kUserVerifyCodeExpired = 1006,
    kUserVerifyCodePhoneMismatch = 1007,
    kUserAlreadyLoggedIn = 1008,
    kUserAccountLocked = 1009,

    kFriendAlreadyExists = 2001,
    kFriendApplyAlreadyExists = 2002,
    kFriendSessionNotFound = 2003,

    kMessageNotFound = 3001,
    kFileNotFound = 4001,
    kSpeechRecognitionFailed = 5001,
    kTransmitTargetNotFound = 6001,
};

inline const char *ErrorCodeName(ErrorCode code) {
    switch (code) {
    case ErrorCode::kInvalidArgument:
        return "INVALID_ARGUMENT";
    case ErrorCode::kUnauthorized:
        return "UNAUTHORIZED";
    case ErrorCode::kForbidden:
        return "FORBIDDEN";
    case ErrorCode::kNotFound:
        return "NOT_FOUND";
    case ErrorCode::kConflict:
        return "CONFLICT";
    case ErrorCode::kDatabaseError:
        return "DATABASE_ERROR";
    case ErrorCode::kRedisError:
        return "REDIS_ERROR";
    case ErrorCode::kExternalServiceError:
        return "EXTERNAL_SERVICE_ERROR";
    case ErrorCode::kTimeout:
        return "TIMEOUT";
    case ErrorCode::kSessionExpired:
        return "SESSION_EXPIRED";
    case ErrorCode::kUserInvalidPhone:
        return "USER_INVALID_PHONE";
    case ErrorCode::kUserInvalidPassword:
        return "USER_INVALID_PASSWORD";
    case ErrorCode::kUserAlreadyExists:
        return "USER_ALREADY_EXISTS";
    case ErrorCode::kUserNotFound:
        return "USER_NOT_FOUND";
    case ErrorCode::kUserVerifyCodeInvalid:
        return "USER_VERIFY_CODE_INVALID";
    case ErrorCode::kUserVerifyCodeExpired:
        return "USER_VERIFY_CODE_EXPIRED";
    case ErrorCode::kUserVerifyCodePhoneMismatch:
        return "USER_VERIFY_CODE_PHONE_MISMATCH";
    case ErrorCode::kUserAlreadyLoggedIn:
        return "USER_ALREADY_LOGGED_IN";
    case ErrorCode::kUserAccountLocked:
        return "USER_ACCOUNT_LOCKED";
    case ErrorCode::kFriendAlreadyExists:
        return "FRIEND_ALREADY_EXISTS";
    case ErrorCode::kFriendApplyAlreadyExists:
        return "FRIEND_APPLY_ALREADY_EXISTS";
    case ErrorCode::kFriendSessionNotFound:
        return "FRIEND_SESSION_NOT_FOUND";
    case ErrorCode::kMessageNotFound:
        return "MESSAGE_NOT_FOUND";
    case ErrorCode::kFileNotFound:
        return "FILE_NOT_FOUND";
    case ErrorCode::kSpeechRecognitionFailed:
        return "SPEECH_RECOGNITION_FAILED";
    case ErrorCode::kTransmitTargetNotFound:
        return "TRANSMIT_TARGET_NOT_FOUND";
    case ErrorCode::kUnknown:
    default:
        return "UNKNOWN";
    }
}

struct ErrorContext {
    std::string key;
    std::string value;
};

struct AppError {
    ErrorCode code = ErrorCode::kUnknown;
    std::string message;
    std::vector<ErrorContext> context;
    std::optional<std::string> detail;

    static AppError FromMessage(std::string message) {
        AppError error;
        error.message = std::move(message);
        return error;
    }

    static AppError WithCode(ErrorCode code, std::string message) {
        AppError error;
        error.code = code;
        error.message = std::move(message);
        return error;
    }

    AppError WithContext(std::string key, std::string value) && {
        context.push_back(ErrorContext{std::move(key), std::move(value)});
        return std::move(*this);
    }

    AppError WithDetail(std::string value) && {
        detail = std::move(value);
        return std::move(*this);
    }
};

inline std::string FormatErrorForClient(const AppError &error) {
    return std::string(ErrorCodeName(error.code)) + ": " + error.message;
}

inline std::string FormatErrorForLog(const AppError &error) {
    std::ostringstream stream;
    stream << ErrorCodeName(error.code) << ": " << error.message;
    if (!error.context.empty()) {
        stream << " context={";
        for (std::size_t i = 0; i < error.context.size(); ++i) {
            if (i > 0) {
                stream << ",";
            }
            stream << error.context[i].key << "=" << error.context[i].value;
        }
        stream << "}";
    }
    if (error.detail.has_value() && !error.detail->empty()) {
        stream << " detail=" << *error.detail;
    }
    return stream.str();
}

template <typename T> class Result {
  public:
    static Result Ok(T value) { return Result(std::move(value)); }

    static Result Fail(std::string message) {
        return Result(AppError::FromMessage(std::move(message)));
    }

    static Result Fail(AppError error) { return Result(std::move(error)); }

    bool ok() const { return std::holds_alternative<T>(data_); }
    const T &value() const { return std::get<T>(data_); }
    T &value() { return std::get<T>(data_); }
    const AppError &error() const { return std::get<AppError>(data_); }

  private:
    explicit Result(T value) : data_(std::move(value)) {}
    explicit Result(AppError error) : data_(std::move(error)) {}

    std::variant<T, AppError> data_;
};

template <> class Result<void> {
  public:
    static Result Ok() { return Result(std::monostate{}); }

    static Result Fail(std::string message) {
        return Result(AppError::FromMessage(std::move(message)));
    }

    static Result Fail(AppError error) { return Result(std::move(error)); }

    bool ok() const { return std::holds_alternative<std::monostate>(data_); }
    const AppError &error() const { return std::get<AppError>(data_); }

  private:
    explicit Result(std::monostate ok) : data_(ok) {}
    explicit Result(AppError error) : data_(std::move(error)) {}

    std::variant<std::monostate, AppError> data_;
};

using VoidResult = Result<void>;

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_COMMON_RESULT_H_
