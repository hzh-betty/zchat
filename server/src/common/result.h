#ifndef ZCHAT_SERVER_SRC_COMMON_RESULT_H_
#define ZCHAT_SERVER_SRC_COMMON_RESULT_H_

#include <optional>
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

    kUserInvalidPhone = 1001,
    kUserInvalidPassword = 1002,
    kUserAlreadyExists = 1003,
    kUserNotFound = 1004,
    kUserVerifyCodeInvalid = 1005,

    kFriendAlreadyExists = 2001,
    kFriendApplyAlreadyExists = 2002,
    kFriendSessionNotFound = 2003,

    kMessageNotFound = 3001,
    kFileNotFound = 4001,
    kSpeechRecognitionFailed = 5001,
    kTransmitTargetNotFound = 6001,
};

struct FieldError {
    std::string field;
    std::string message;
};

struct AppError {
    ErrorCode code = ErrorCode::kUnknown;
    std::string message;
    std::vector<FieldError> fields;
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
};

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
