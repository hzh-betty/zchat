#include "user/user_service.h"

#include <algorithm>
#include <cctype>
#include <vector>

#include "common/error_response.h"
#include "common/logger.h"
#include "common/proto_mapper.h"
#include "common/uuid.h"

namespace zchat {
namespace {

template <typename Response>
Response ErrorResponse(const std::string &request_id,
                       const AppError &error) {
    return MakeErrorResponse<Response>(request_id, error);
}

template <typename Response>
Response ErrorResponse(const std::string &request_id, ErrorCode code,
                       const std::string &message) {
    return MakeErrorResponse<Response>(request_id, code, message);
}

template <typename Response>
void MarkOk(const std::string &request_id, Response *response) {
    response->set_request_id(request_id);
    response->set_success(true);
    response->set_errmsg("");
}

} // namespace

UserApplicationService::UserApplicationService(UserRepository &users,
                                               FileRepository &files,
                                               SmsClient &sms,
                                               SessionStore &sessions,
                                               UserSearchIndex &search_index)
    : users_(users), files_(files), sms_(sms), sessions_(sessions),
      search_index_(search_index) {}

zchat::UserRegisterRsp UserApplicationService::RegisterByNickname(
    const zchat::UserRegisterReq &request) {
    ZCHAT_LOG_INFO("RegisterByNickname request_id={}", request.request_id());
    if (request.nickname().empty()) {
        return ErrorResponse<zchat::UserRegisterRsp>(
            request.request_id(), ErrorCode::kInvalidArgument,
            "nickname is required");
    }
    if (!IsValidPassword(request.password())) {
        return ErrorResponse<zchat::UserRegisterRsp>(
            request.request_id(), ErrorCode::kUserInvalidPassword,
            "invalid password format");
    }
    auto existing = users_.FindUserByNickname(request.nickname());
    if (!existing.ok()) {
        return ErrorResponse<zchat::UserRegisterRsp>(request.request_id(),
                                                     existing.error());
    }
    if (existing.value().has_value()) {
        return ErrorResponse<zchat::UserRegisterRsp>(
            request.request_id(), ErrorCode::kUserAlreadyExists,
            "nickname already exists");
    }

    UserRecord user;
    user.user_id = NewId();
    user.nickname = request.nickname();
    user.password = request.password();
    const auto inserted = users_.InsertUser(user);
    if (!inserted.ok()) {
        return ErrorResponse<zchat::UserRegisterRsp>(request.request_id(),
                                                     inserted.error());
    }
    IndexUser(user);

    zchat::UserRegisterRsp response;
    MarkOk(request.request_id(), &response);
    ZCHAT_LOG_INFO("RegisterByNickname success: request_id={}", request.request_id());
    return response;
}

zchat::UserLoginRsp
UserApplicationService::LoginByNickname(const zchat::UserLoginReq &request) {
    ZCHAT_LOG_INFO("LoginByNickname request_id={}", request.request_id());
    auto user = users_.FindUserByNickname(request.nickname());
    if (!user.ok()) {
        return ErrorResponse<zchat::UserLoginRsp>(request.request_id(),
                                                  user.error());
    }
    if (!user.value().has_value() ||
        user.value()->password != request.password()) {
        return ErrorResponse<zchat::UserLoginRsp>(
            request.request_id(), ErrorCode::kUnauthorized,
            "invalid nickname or password");
    }

    auto session_id = LoginUser(user.value()->user_id);
    if (!session_id.ok()) {
        return ErrorResponse<zchat::UserLoginRsp>(request.request_id(),
                                                  session_id.error());
    }
    zchat::UserLoginRsp response;
    MarkOk(request.request_id(), &response);
    response.set_login_session_id(session_id.value());
    ZCHAT_LOG_INFO("LoginByNickname success: request_id={} user_id={}", request.request_id(), user.value()->user_id);
    return response;
}

zchat::PhoneVerifyCodeRsp UserApplicationService::GetPhoneVerifyCode(
    const zchat::PhoneVerifyCodeReq &request) {
    ZCHAT_LOG_INFO("GetPhoneVerifyCode request_id={}", request.request_id());
    if (!IsValidPhone(request.phone_number())) {
        return ErrorResponse<zchat::PhoneVerifyCodeRsp>(
            request.request_id(), ErrorCode::kUserInvalidPhone,
            "invalid phone number");
    }
    const std::string verify_code_id = NewId();
    const auto saved =
        sessions_.SaveVerifyCode(verify_code_id, request.phone_number());
    if (!saved.ok()) {
        return ErrorResponse<zchat::PhoneVerifyCodeRsp>(request.request_id(),
                                                         saved.error());
    }
    const auto sent = sms_.SendVerificationCode(request.phone_number());
    if (!sent.ok()) {
        sessions_.RemoveVerifyCode(verify_code_id);
        return ErrorResponse<zchat::PhoneVerifyCodeRsp>(request.request_id(),
                                                         sent.error());
    }
    zchat::PhoneVerifyCodeRsp response;
    MarkOk(request.request_id(), &response);
    response.set_verify_code_id(verify_code_id);
    ZCHAT_LOG_INFO("GetPhoneVerifyCode success: request_id={} phone={}", request.request_id(), request.phone_number());
    return response;
}

zchat::PhoneRegisterRsp UserApplicationService::RegisterByPhone(
    const zchat::PhoneRegisterReq &request) {
    ZCHAT_LOG_INFO("RegisterByPhone request_id={}", request.request_id());
    if (!IsValidPhone(request.phone_number())) {
        return ErrorResponse<zchat::PhoneRegisterRsp>(
            request.request_id(), ErrorCode::kUserInvalidPhone,
            "invalid phone number");
    }
    auto existing_user = users_.FindUserByPhone(request.phone_number());
    if (!existing_user.ok()) {
        return ErrorResponse<zchat::PhoneRegisterRsp>(
            request.request_id(), existing_user.error());
    }
    if (existing_user.value().has_value()) {
        return ErrorResponse<zchat::PhoneRegisterRsp>(
            request.request_id(), ErrorCode::kUserAlreadyExists,
            "phone number already registered");
    }
    const auto code =
        ValidateVerifyCode(request.verify_code_id(), request.verify_code());
    if (!code.ok()) {
        return ErrorResponse<zchat::PhoneRegisterRsp>(request.request_id(),
                                                      code.error());
    }
    if (code.value() != request.phone_number()) {
        return ErrorResponse<zchat::PhoneRegisterRsp>(
            request.request_id(), ErrorCode::kUserVerifyCodeInvalid,
            "verification code does not match phone number");
    }
    auto existing = users_.FindUserByPhone(request.phone_number());
    if (!existing.ok()) {
        return ErrorResponse<zchat::PhoneRegisterRsp>(request.request_id(),
                                                      existing.error());
    }
    if (existing.value().has_value()) {
        return ErrorResponse<zchat::PhoneRegisterRsp>(
            request.request_id(), ErrorCode::kUserAlreadyExists,
            "phone number already registered");
    }
    UserRecord user;
    user.user_id = NewId();
    user.nickname = user.user_id;
    user.phone = request.phone_number();
    const auto inserted = users_.InsertUser(user);
    if (!inserted.ok()) {
        return ErrorResponse<zchat::PhoneRegisterRsp>(request.request_id(),
                                                      inserted.error());
    }
    IndexUser(user);
    sessions_.RemoveVerifyCode(request.verify_code_id());
    zchat::PhoneRegisterRsp response;
    MarkOk(request.request_id(), &response);
    ZCHAT_LOG_INFO("RegisterByPhone success: request_id={}", request.request_id());
    return response;
}

zchat::PhoneLoginRsp
UserApplicationService::LoginByPhone(const zchat::PhoneLoginReq &request) {
    ZCHAT_LOG_INFO("LoginByPhone request_id={}", request.request_id());
    if (!IsValidPhone(request.phone_number())) {
        return ErrorResponse<zchat::PhoneLoginRsp>(
            request.request_id(), ErrorCode::kUserInvalidPhone,
            "invalid phone number");
    }
    auto existing_user = users_.FindUserByPhone(request.phone_number());
    if (!existing_user.ok()) {
        return ErrorResponse<zchat::PhoneLoginRsp>(
            request.request_id(), existing_user.error());
    }
    if (!existing_user.value().has_value()) {
        return ErrorResponse<zchat::PhoneLoginRsp>(
            request.request_id(), ErrorCode::kUserNotFound,
            "phone number is not registered");
    }
    const auto code =
        ValidateVerifyCode(request.verify_code_id(), request.verify_code());
    if (!code.ok()) {
        return ErrorResponse<zchat::PhoneLoginRsp>(request.request_id(),
                                                   code.error());
    }
    if (code.value() != request.phone_number()) {
        return ErrorResponse<zchat::PhoneLoginRsp>(
            request.request_id(), ErrorCode::kUserVerifyCodeInvalid,
            "verification code does not match phone number");
    }
    auto user = users_.FindUserByPhone(request.phone_number());
    if (!user.ok()) {
        return ErrorResponse<zchat::PhoneLoginRsp>(request.request_id(),
                                                   user.error());
    }
    if (!user.value().has_value()) {
        return ErrorResponse<zchat::PhoneLoginRsp>(
            request.request_id(), ErrorCode::kUserNotFound,
            "phone number is not registered");
    }
    auto session_id = LoginUser(user.value()->user_id);
    if (!session_id.ok()) {
        return ErrorResponse<zchat::PhoneLoginRsp>(request.request_id(),
                                                   session_id.error());
    }
    zchat::PhoneLoginRsp response;
    MarkOk(request.request_id(), &response);
    response.set_login_session_id(session_id.value());
    sessions_.RemoveVerifyCode(request.verify_code_id());
    ZCHAT_LOG_INFO("LoginByPhone success: request_id={}", request.request_id());
    return response;
}

zchat::GetUserInfoRsp
UserApplicationService::GetUserInfo(const zchat::GetUserInfoReq &request) {
    ZCHAT_LOG_INFO("GetUserInfo request_id={}", request.request_id());
    const auto user_id = UserIdFromSession(request.session_id());
    if (!user_id.ok()) {
        return ErrorResponse<zchat::GetUserInfoRsp>(request.request_id(),
                                                    user_id.error());
    }
    auto user = users_.FindUserById(user_id.value());
    if (!user.ok()) {
        return ErrorResponse<zchat::GetUserInfoRsp>(request.request_id(),
                                                    user.error());
    }
    if (!user.value().has_value()) {
        return ErrorResponse<zchat::GetUserInfoRsp>(
            request.request_id(), ErrorCode::kUserNotFound, "user not found");
    }
    std::string avatar;
    if (!user.value()->avatar_id.empty()) {
        auto file = files_.GetFile(user.value()->avatar_id);
        if (file.ok() && file.value().has_value()) {
            avatar = file.value()->file_content;
        }
    }
    zchat::GetUserInfoRsp response;
    MarkOk(request.request_id(), &response);
    *response.mutable_user_info() = ToProtoUser(user.value().value(), avatar);
    ZCHAT_LOG_INFO("GetUserInfo success: request_id={} user_id={}", request.request_id(), user_id.value());
    return response;
}

zchat::GetMultiUserInfoRsp UserApplicationService::GetMultiUserInfo(
    const zchat::GetMultiUserInfoReq &request) {
    zchat::GetMultiUserInfoRsp response;
    response.set_request_id(request.request_id());
    auto users = users_.FindUsersByIds(std::vector<std::string>(
        request.users_id().begin(), request.users_id().end()));
    if (!users.ok()) {
        response.set_success(false);
        response.set_errmsg(FormatErrorForClient(users.error()));
        return response;
    }
    response.set_success(true);
    response.set_errmsg("");
    for (const auto &user : users.value()) {
        std::string avatar;
        if (!user.avatar_id.empty()) {
            auto file = files_.GetFile(user.avatar_id);
            if (file.ok() && file.value().has_value()) {
                avatar = file.value()->file_content;
            }
        }
        (*response.mutable_users_info())[user.user_id] =
            ToProtoUser(user, avatar);
    }
    return response;
}

zchat::SetUserAvatarRsp
UserApplicationService::SetAvatar(const zchat::SetUserAvatarReq &request) {
    ZCHAT_LOG_INFO("SetAvatar request_id={}", request.request_id());
    const auto user_id = UserIdFromSession(request.session_id());
    if (!user_id.ok()) {
        return ErrorResponse<zchat::SetUserAvatarRsp>(request.request_id(),
                                                      user_id.error());
    }
    const std::string file_id = NewId();
    const auto file_result = files_.PutFile(FileRecord{
        file_id, "avatar", request.avatar().size(), request.avatar()});
    if (!file_result.ok()) {
        return ErrorResponse<zchat::SetUserAvatarRsp>(
            request.request_id(), file_result.error());
    }
    const auto updated = users_.UpdateUserAvatar(user_id.value(), file_id);
    if (!updated.ok()) {
        return ErrorResponse<zchat::SetUserAvatarRsp>(request.request_id(),
                                                      updated.error());
    }
    IndexUserById(user_id.value());
    zchat::SetUserAvatarRsp response;
    MarkOk(request.request_id(), &response);
    ZCHAT_LOG_INFO("SetAvatar success: request_id={}", request.request_id());
    return response;
}

zchat::SetUserNicknameRsp
UserApplicationService::SetNickname(const zchat::SetUserNicknameReq &request) {
    ZCHAT_LOG_INFO("SetNickname request_id={}", request.request_id());
    const auto user_id = UserIdFromSession(request.session_id());
    if (!user_id.ok()) {
        return ErrorResponse<zchat::SetUserNicknameRsp>(
            request.request_id(), user_id.error());
    }
    const auto updated =
        users_.UpdateUserNickname(user_id.value(), request.nickname());
    if (!updated.ok()) {
        return ErrorResponse<zchat::SetUserNicknameRsp>(
            request.request_id(), updated.error());
    }
    IndexUserById(user_id.value());
    zchat::SetUserNicknameRsp response;
    MarkOk(request.request_id(), &response);
    ZCHAT_LOG_INFO("SetNickname success: request_id={}", request.request_id());
    return response;
}

zchat::SetUserDescriptionRsp UserApplicationService::SetDescription(
    const zchat::SetUserDescriptionReq &request) {
    ZCHAT_LOG_INFO("SetDescription request_id={}", request.request_id());
    const auto user_id = UserIdFromSession(request.session_id());
    if (!user_id.ok()) {
        return ErrorResponse<zchat::SetUserDescriptionRsp>(
            request.request_id(), user_id.error());
    }
    const auto updated =
        users_.UpdateUserDescription(user_id.value(), request.description());
    if (!updated.ok()) {
        return ErrorResponse<zchat::SetUserDescriptionRsp>(
            request.request_id(), updated.error());
    }
    IndexUserById(user_id.value());
    zchat::SetUserDescriptionRsp response;
    MarkOk(request.request_id(), &response);
    ZCHAT_LOG_INFO("SetDescription success: request_id={}", request.request_id());
    return response;
}

zchat::SetUserPhoneNumberRsp
UserApplicationService::SetPhone(const zchat::SetUserPhoneNumberReq &request) {
    ZCHAT_LOG_INFO("SetPhone request_id={}", request.request_id());
    const auto user_id = UserIdFromSession(request.session_id());
    if (!user_id.ok()) {
        return ErrorResponse<zchat::SetUserPhoneNumberRsp>(
            request.request_id(), user_id.error());
    }
    const auto code = ValidateVerifyCode(request.phone_verify_code_id(),
                                         request.phone_verify_code());
    if (!code.ok()) {
        return ErrorResponse<zchat::SetUserPhoneNumberRsp>(
            request.request_id(), code.error());
    }
    if (code.value() != request.phone_number()) {
        return ErrorResponse<zchat::SetUserPhoneNumberRsp>(
            request.request_id(), ErrorCode::kUserVerifyCodeInvalid,
            "verification code does not match phone number");
    }
    const auto updated =
        users_.UpdateUserPhone(user_id.value(), request.phone_number());
    if (!updated.ok()) {
        return ErrorResponse<zchat::SetUserPhoneNumberRsp>(
            request.request_id(), updated.error());
    }
    IndexUserById(user_id.value());
    sessions_.RemoveVerifyCode(request.phone_verify_code_id());
    zchat::SetUserPhoneNumberRsp response;
    MarkOk(request.request_id(), &response);
    ZCHAT_LOG_INFO("SetPhone success: request_id={}", request.request_id());
    return response;
}

Result<std::string>
UserApplicationService::UserIdFromSession(const std::string &session_id) {
    if (session_id.empty()) {
        return Result<std::string>::Fail(AppError::WithCode(
            ErrorCode::kUnauthorized, "session id is required"));
    }
    auto user_id = sessions_.GetUserId(session_id);
    if (!user_id.ok()) {
        return Result<std::string>::Fail(user_id.error());
    }
    if (!user_id.value().has_value()) {
        return Result<std::string>::Fail(AppError::WithCode(
            ErrorCode::kUnauthorized, "session expired"));
    }
    return Result<std::string>::Ok(user_id.value().value());
}

Result<std::string>
UserApplicationService::ValidateVerifyCode(const std::string &verify_code_id,
                                           const std::string &verify_code) {
    auto saved = sessions_.GetVerifyCode(verify_code_id);
    if (!saved.ok()) {
        return Result<std::string>::Fail(saved.error());
    }
    if (!saved.value().has_value()) {
        return Result<std::string>::Fail(AppError::WithCode(
            ErrorCode::kUserVerifyCodeInvalid, "verification code expired"));
    }
    const std::string &phone = saved.value().value();
    auto checked = sms_.CheckVerificationCode(phone, verify_code);
    if (!checked.ok()) {
        return Result<std::string>::Fail(checked.error());
    }
    return Result<std::string>::Ok(phone);
}

bool UserApplicationService::IsValidPhone(const std::string &phone) const {
    return phone.size() == 11 && phone[0] == '1' &&
           std::all_of(phone.begin(), phone.end(),
                       [](unsigned char c) { return std::isdigit(c) != 0; });
}

bool UserApplicationService::IsValidPassword(
    const std::string &password) const {
    if (password.size() < 6 || password.size() > 32) {
        return false;
    }
    return std::all_of(password.begin(), password.end(), [](unsigned char c) {
        return std::isalnum(c) != 0 || c == '_' || c == '-';
    });
}

void UserApplicationService::IndexUser(const UserRecord &user) {
    const auto indexed = search_index_.IndexUser(user);
    if (!indexed.ok()) {
        ZCHAT_LOG_WARN("user es index failed: user_id={} error={}",
                       user.user_id, indexed.error().message);
    }
}

void UserApplicationService::IndexUserById(const std::string &user_id) {
    auto user = users_.FindUserById(user_id);
    if (!user.ok() || !user.value().has_value()) {
        ZCHAT_LOG_WARN("user es index skipped: user_id={}", user_id);
        return;
    }
    IndexUser(user.value().value());
}

Result<std::string> UserApplicationService::LoginUser(
    const std::string &user_id) {
    auto online = sessions_.IsOnline(user_id);
    if (!online.ok()) {
        return Result<std::string>::Fail(online.error());
    }
    if (online.value()) {
        return Result<std::string>::Fail(AppError::WithCode(
            ErrorCode::kConflict, "user already logged in"));
    }
    const std::string session_id = NewId();
    auto saved = sessions_.SaveSession(session_id, user_id);
    if (!saved.ok()) {
        return Result<std::string>::Fail(saved.error());
    }
    auto online_set = sessions_.SetOnline(user_id);
    if (!online_set.ok()) {
        sessions_.RemoveSession(session_id);
        return Result<std::string>::Fail(online_set.error());
    }
    return Result<std::string>::Ok(session_id);
}

} // namespace zchat
