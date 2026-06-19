#include "user/user_service.h"

#include <algorithm>
#include <cctype>
#include <vector>

#include "common/common_errors.h"
#include "common/crypto.h"
#include "common/error_response.h"
#include "common/logger.h"
#include "common/proto_mapper.h"
#include "common/uuid.h"
#include "user/user_errors.h"

namespace zchat {
namespace {

template <typename Response>
Response ErrorResponse(const std::string &request_id, const AppError &error) {
    return MakeErrorResponse<Response>(request_id, error);
}

template <typename Response>
void MarkOk(const std::string &request_id, Response *response) {
    response->set_request_id(request_id);
    response->set_success(true);
    response->set_errmsg("");
}

} // namespace

UserApplicationService::UserApplicationService(UserRepository &users,
                                               ServiceClients &clients,
                                               SmsClient &sms,
                                               SessionStore &sessions,
                                               UserSearchIndex &search_index)
    : users_(users), clients_(clients), sms_(sms), sessions_(sessions),
      search_index_(search_index) {}

drogon::Task<zchat::UserRegisterRsp>
UserApplicationService::RegisterByNicknameCoro(
    const zchat::UserRegisterReq &request) {
    ZCHAT_LOG_INFO("RegisterByNickname request_id={}", request.request_id());
    if (request.nickname().empty()) {
        co_return ErrorResponse<zchat::UserRegisterRsp>(
            request.request_id(), user_errors::NicknameRequired());
    }
    if (!IsValidPassword(request.password())) {
        co_return ErrorResponse<zchat::UserRegisterRsp>(
            request.request_id(), user_errors::InvalidPassword());
    }
    auto existing = co_await users_.FindUserByNicknameCoro(request.nickname());
    if (!existing.ok()) {
        co_return ErrorResponse<zchat::UserRegisterRsp>(request.request_id(),
                                                        existing.error());
    }
    if (existing.value().has_value()) {
        co_return ErrorResponse<zchat::UserRegisterRsp>(
            request.request_id(), user_errors::NicknameAlreadyExists());
    }

    UserRecord user;
    user.user_id = NewId();
    user.nickname = request.nickname();
    user.password = Argon2idHash(request.password());
    const auto inserted = co_await users_.InsertUserCoro(user);
    if (!inserted.ok()) {
        co_return ErrorResponse<zchat::UserRegisterRsp>(request.request_id(),
                                                        inserted.error());
    }
    const auto indexed = co_await IndexUserCoro(user);
    if (!indexed.ok()) {
        co_return ErrorResponse<zchat::UserRegisterRsp>(request.request_id(),
                                                        indexed.error());
    }

    zchat::UserRegisterRsp response;
    MarkOk(request.request_id(), &response);
    co_return response;
}

drogon::Task<zchat::UserLoginRsp> UserApplicationService::LoginByNicknameCoro(
    const zchat::UserLoginReq &request) {
    ZCHAT_LOG_INFO("LoginByNickname request_id={}", request.request_id());
    auto user = co_await users_.FindUserByNicknameCoro(request.nickname());
    if (!user.ok()) {
        co_return ErrorResponse<zchat::UserLoginRsp>(request.request_id(),
                                                     user.error());
    }
    if (!user.value().has_value()) {
        co_return ErrorResponse<zchat::UserLoginRsp>(
            request.request_id(), user_errors::InvalidCredentials());
    }

    const std::string &uid = user.value()->user_id;

    auto locked = co_await sessions_.IsAccountLockedCoro(uid);
    if (locked.ok() && locked.value()) {
        co_return ErrorResponse<zchat::UserLoginRsp>(
            request.request_id(), user_errors::AccountLocked());
    }

    const auto &stored = user.value()->password;
    bool password_ok =
        !stored.empty() && Argon2idVerify(stored, request.password());

    if (!password_ok) {
        auto fail = co_await sessions_.RecordLoginFailCoro(uid);
        if (fail.ok() && fail.value() >= 5) {
            co_return ErrorResponse<zchat::UserLoginRsp>(
                request.request_id(), user_errors::AccountLocked());
        }
        co_return ErrorResponse<zchat::UserLoginRsp>(
            request.request_id(), user_errors::InvalidCredentials());
    }

    co_await sessions_.ClearLoginFailCoro(uid);
    auto session_id = co_await LoginUserCoro(uid);
    if (!session_id.ok()) {
        co_return ErrorResponse<zchat::UserLoginRsp>(request.request_id(),
                                                     session_id.error());
    }
    zchat::UserLoginRsp response;
    MarkOk(request.request_id(), &response);
    response.set_login_session_id(session_id.value());
    co_return response;
}

drogon::Task<zchat::PhoneVerifyCodeRsp>
UserApplicationService::GetPhoneVerifyCodeCoro(
    const zchat::PhoneVerifyCodeReq &request) {
    ZCHAT_LOG_INFO("GetPhoneVerifyCode request_id={}", request.request_id());
    if (!IsValidPhone(request.phone_number())) {
        co_return ErrorResponse<zchat::PhoneVerifyCodeRsp>(
            request.request_id(), user_errors::InvalidPhone());
    }
    const auto rate_ok = co_await sessions_.RateLimitCoro(
        "sms:phone:" + request.phone_number(), 60, 1);
    if (rate_ok.ok() && !rate_ok.value()) {
        co_return ErrorResponse<zchat::PhoneVerifyCodeRsp>(
            request.request_id(),
            AppError::WithCode(ErrorCode::kInvalidArgument,
                               "verification code requests too frequent"));
    }
    const std::string verify_code_id = NewId();
    const auto saved = co_await sessions_.SaveVerifyCodeCoro(
        verify_code_id, request.phone_number());
    if (!saved.ok()) {
        co_return ErrorResponse<zchat::PhoneVerifyCodeRsp>(request.request_id(),
                                                           saved.error());
    }
    const auto sent =
        co_await sms_.SendVerificationCode(request.phone_number());
    if (!sent.ok()) {
        co_await sessions_.RemoveVerifyCodeCoro(verify_code_id);
        co_return ErrorResponse<zchat::PhoneVerifyCodeRsp>(request.request_id(),
                                                           sent.error());
    }
    zchat::PhoneVerifyCodeRsp response;
    MarkOk(request.request_id(), &response);
    response.set_verify_code_id(verify_code_id);
    co_return response;
}

drogon::Task<zchat::PhoneRegisterRsp>
UserApplicationService::RegisterByPhoneCoro(
    const zchat::PhoneRegisterReq &request) {
    ZCHAT_LOG_INFO("RegisterByPhone request_id={}", request.request_id());
    if (!IsValidPhone(request.phone_number())) {
        co_return ErrorResponse<zchat::PhoneRegisterRsp>(
            request.request_id(), user_errors::InvalidPhone());
    }
    auto existing_user =
        co_await users_.FindUserByPhoneCoro(request.phone_number());
    if (!existing_user.ok()) {
        co_return ErrorResponse<zchat::PhoneRegisterRsp>(request.request_id(),
                                                         existing_user.error());
    }
    if (existing_user.value().has_value()) {
        co_return ErrorResponse<zchat::PhoneRegisterRsp>(
            request.request_id(), user_errors::PhoneAlreadyRegistered());
    }
    const auto code = co_await ValidateVerifyCodeCoro(request.verify_code_id(),
                                                      request.verify_code());
    if (!code.ok()) {
        co_return ErrorResponse<zchat::PhoneRegisterRsp>(request.request_id(),
                                                         code.error());
    }
    if (code.value() != request.phone_number()) {
        co_return ErrorResponse<zchat::PhoneRegisterRsp>(
            request.request_id(), user_errors::VerifyCodePhoneMismatch());
    }
    UserRecord user;
    user.user_id = NewId();
    user.nickname = user.user_id;
    user.phone = request.phone_number();
    const auto inserted = co_await users_.InsertUserCoro(user);
    if (!inserted.ok()) {
        co_return ErrorResponse<zchat::PhoneRegisterRsp>(request.request_id(),
                                                         inserted.error());
    }
    const auto indexed = co_await IndexUserCoro(user);
    if (!indexed.ok()) {
        co_return ErrorResponse<zchat::PhoneRegisterRsp>(request.request_id(),
                                                         indexed.error());
    }
    co_await sessions_.RemoveVerifyCodeCoro(request.verify_code_id());
    zchat::PhoneRegisterRsp response;
    MarkOk(request.request_id(), &response);
    co_return response;
}

drogon::Task<zchat::PhoneLoginRsp>
UserApplicationService::LoginByPhoneCoro(const zchat::PhoneLoginReq &request) {
    ZCHAT_LOG_INFO("LoginByPhone request_id={}", request.request_id());
    if (!IsValidPhone(request.phone_number())) {
        co_return ErrorResponse<zchat::PhoneLoginRsp>(
            request.request_id(), user_errors::InvalidPhone());
    }
    auto existing_user =
        co_await users_.FindUserByPhoneCoro(request.phone_number());
    if (!existing_user.ok()) {
        co_return ErrorResponse<zchat::PhoneLoginRsp>(request.request_id(),
                                                      existing_user.error());
    }
    if (!existing_user.value().has_value()) {
        co_return ErrorResponse<zchat::PhoneLoginRsp>(
            request.request_id(), user_errors::PhoneNotRegistered());
    }
    const auto code = co_await ValidateVerifyCodeCoro(request.verify_code_id(),
                                                      request.verify_code());
    if (!code.ok()) {
        co_return ErrorResponse<zchat::PhoneLoginRsp>(request.request_id(),
                                                      code.error());
    }
    if (code.value() != request.phone_number()) {
        co_return ErrorResponse<zchat::PhoneLoginRsp>(
            request.request_id(), user_errors::VerifyCodePhoneMismatch());
    }
    auto user = co_await users_.FindUserByPhoneCoro(request.phone_number());
    if (!user.ok()) {
        co_return ErrorResponse<zchat::PhoneLoginRsp>(request.request_id(),
                                                      user.error());
    }
    if (!user.value().has_value()) {
        co_return ErrorResponse<zchat::PhoneLoginRsp>(
            request.request_id(), user_errors::PhoneNotRegistered());
    }
    auto session_id = co_await LoginUserCoro(user.value()->user_id);
    if (!session_id.ok()) {
        co_return ErrorResponse<zchat::PhoneLoginRsp>(request.request_id(),
                                                      session_id.error());
    }
    zchat::PhoneLoginRsp response;
    MarkOk(request.request_id(), &response);
    response.set_login_session_id(session_id.value());
    co_await sessions_.RemoveVerifyCodeCoro(request.verify_code_id());
    co_return response;
}

drogon::Task<zchat::GetUserInfoRsp>
UserApplicationService::GetUserInfoCoro(const zchat::GetUserInfoReq &request) {
    ZCHAT_LOG_INFO("GetUserInfo request_id={}", request.request_id());
    const auto user_id = co_await UserIdFromSessionCoro(request.session_id());
    if (!user_id.ok()) {
        co_return ErrorResponse<zchat::GetUserInfoRsp>(request.request_id(),
                                                       user_id.error());
    }
    auto user = co_await users_.FindUserByIdCoro(user_id.value());
    if (!user.ok()) {
        co_return ErrorResponse<zchat::GetUserInfoRsp>(request.request_id(),
                                                       user.error());
    }
    if (!user.value().has_value()) {
        co_return ErrorResponse<zchat::GetUserInfoRsp>(
            request.request_id(), user_errors::UserNotFound());
    }
    std::string avatar = co_await GetAvatarContentCoro(user.value()->avatar_id);
    zchat::GetUserInfoRsp response;
    MarkOk(request.request_id(), &response);
    *response.mutable_user_info() = ToProtoUser(user.value().value(), avatar);
    co_return response;
}

drogon::Task<zchat::GetMultiUserInfoRsp>
UserApplicationService::GetMultiUserInfoCoro(
    const zchat::GetMultiUserInfoReq &request) {
    zchat::GetMultiUserInfoRsp response;
    response.set_request_id(request.request_id());
    auto users = co_await users_.FindUsersByIdsCoro(std::vector<std::string>(
        request.users_id().begin(), request.users_id().end()));
    if (!users.ok()) {
        response.set_success(false);
        response.set_errmsg(FormatErrorForClient(users.error()));
        co_return response;
    }
    response.set_success(true);
    response.set_errmsg("");
    for (const auto &user : users.value()) {
        std::string avatar = co_await GetAvatarContentCoro(user.avatar_id);
        (*response.mutable_users_info())[user.user_id] =
            ToProtoUser(user, avatar);
    }
    co_return response;
}

drogon::Task<zchat::SearchUsersRsp>
UserApplicationService::SearchUsersCoro(const zchat::SearchUsersReq &request) {
    ZCHAT_LOG_INFO("SearchUsers request_id={}", request.request_id());
    auto users = co_await search_index_.SearchUsersCoro(
        request.search_key(), {request.exclude_user_id()});
    if (!users.ok()) {
        co_return ErrorResponse<zchat::SearchUsersRsp>(request.request_id(),
                                                       users.error());
    }
    zchat::SearchUsersRsp response;
    response.set_request_id(request.request_id());
    response.set_success(true);
    response.set_errmsg("");
    for (const auto &user : users.value()) {
        std::string avatar = co_await GetAvatarContentCoro(user.avatar_id);
        *response.add_user_info() = ToProtoUser(user, avatar);
    }
    co_return response;
}

drogon::Task<zchat::SetUserAvatarRsp>
UserApplicationService::SetAvatarCoro(const zchat::SetUserAvatarReq &request) {
    ZCHAT_LOG_INFO("SetAvatar request_id={}", request.request_id());
    const auto user_id = co_await UserIdFromSessionCoro(request.session_id());
    if (!user_id.ok()) {
        co_return ErrorResponse<zchat::SetUserAvatarRsp>(request.request_id(),
                                                         user_id.error());
    }
    const auto file_id = co_await PutAvatarContentCoro(request.avatar());
    if (!file_id.ok()) {
        co_return ErrorResponse<zchat::SetUserAvatarRsp>(request.request_id(),
                                                         file_id.error());
    }
    const auto updated =
        co_await users_.UpdateUserAvatarCoro(user_id.value(), file_id.value());
    if (!updated.ok()) {
        co_return ErrorResponse<zchat::SetUserAvatarRsp>(request.request_id(),
                                                         updated.error());
    }
    const auto indexed = co_await IndexUserByIdCoro(user_id.value());
    if (!indexed.ok()) {
        co_return ErrorResponse<zchat::SetUserAvatarRsp>(request.request_id(),
                                                         indexed.error());
    }
    zchat::SetUserAvatarRsp response;
    MarkOk(request.request_id(), &response);
    co_return response;
}

drogon::Task<zchat::SetUserNicknameRsp> UserApplicationService::SetNicknameCoro(
    const zchat::SetUserNicknameReq &request) {
    ZCHAT_LOG_INFO("SetNickname request_id={}", request.request_id());
    const auto user_id = co_await UserIdFromSessionCoro(request.session_id());
    if (!user_id.ok()) {
        co_return ErrorResponse<zchat::SetUserNicknameRsp>(request.request_id(),
                                                           user_id.error());
    }
    const auto updated = co_await users_.UpdateUserNicknameCoro(
        user_id.value(), request.nickname());
    if (!updated.ok()) {
        co_return ErrorResponse<zchat::SetUserNicknameRsp>(request.request_id(),
                                                           updated.error());
    }
    const auto indexed = co_await IndexUserByIdCoro(user_id.value());
    if (!indexed.ok()) {
        co_return ErrorResponse<zchat::SetUserNicknameRsp>(request.request_id(),
                                                           indexed.error());
    }
    zchat::SetUserNicknameRsp response;
    MarkOk(request.request_id(), &response);
    co_return response;
}

drogon::Task<zchat::SetUserDescriptionRsp>
UserApplicationService::SetDescriptionCoro(
    const zchat::SetUserDescriptionReq &request) {
    ZCHAT_LOG_INFO("SetDescription request_id={}", request.request_id());
    const auto user_id = co_await UserIdFromSessionCoro(request.session_id());
    if (!user_id.ok()) {
        co_return ErrorResponse<zchat::SetUserDescriptionRsp>(
            request.request_id(), user_id.error());
    }
    const auto updated = co_await users_.UpdateUserDescriptionCoro(
        user_id.value(), request.description());
    if (!updated.ok()) {
        co_return ErrorResponse<zchat::SetUserDescriptionRsp>(
            request.request_id(), updated.error());
    }
    const auto indexed = co_await IndexUserByIdCoro(user_id.value());
    if (!indexed.ok()) {
        co_return ErrorResponse<zchat::SetUserDescriptionRsp>(
            request.request_id(), indexed.error());
    }
    zchat::SetUserDescriptionRsp response;
    MarkOk(request.request_id(), &response);
    co_return response;
}

drogon::Task<zchat::SetUserPhoneNumberRsp> UserApplicationService::SetPhoneCoro(
    const zchat::SetUserPhoneNumberReq &request) {
    ZCHAT_LOG_INFO("SetPhone request_id={}", request.request_id());
    const auto user_id = co_await UserIdFromSessionCoro(request.session_id());
    if (!user_id.ok()) {
        co_return ErrorResponse<zchat::SetUserPhoneNumberRsp>(
            request.request_id(), user_id.error());
    }
    const auto code = co_await ValidateVerifyCodeCoro(
        request.phone_verify_code_id(), request.phone_verify_code());
    if (!code.ok()) {
        co_return ErrorResponse<zchat::SetUserPhoneNumberRsp>(
            request.request_id(), code.error());
    }
    if (code.value() != request.phone_number()) {
        co_return ErrorResponse<zchat::SetUserPhoneNumberRsp>(
            request.request_id(), user_errors::VerifyCodePhoneMismatch());
    }
    const auto updated = co_await users_.UpdateUserPhoneCoro(
        user_id.value(), request.phone_number());
    if (!updated.ok()) {
        co_return ErrorResponse<zchat::SetUserPhoneNumberRsp>(
            request.request_id(), updated.error());
    }
    const auto indexed = co_await IndexUserByIdCoro(user_id.value());
    if (!indexed.ok()) {
        co_return ErrorResponse<zchat::SetUserPhoneNumberRsp>(
            request.request_id(), indexed.error());
    }
    co_await sessions_.RemoveVerifyCodeCoro(request.phone_verify_code_id());
    zchat::SetUserPhoneNumberRsp response;
    MarkOk(request.request_id(), &response);
    co_return response;
}

drogon::Task<Result<std::string>>
UserApplicationService::UserIdFromSessionCoro(const std::string &session_id) {
    if (session_id.empty()) {
        co_return Result<std::string>::Fail(common_errors::SessionIdRequired());
    }
    auto user_id = co_await sessions_.GetUserIdCoro(session_id);
    if (!user_id.ok()) {
        co_return Result<std::string>::Fail(user_id.error());
    }
    if (!user_id.value().has_value()) {
        co_return Result<std::string>::Fail(common_errors::SessionExpired());
    }
    co_return Result<std::string>::Ok(user_id.value().value());
}

drogon::Task<Result<std::string>>
UserApplicationService::ValidateVerifyCodeCoro(
    const std::string &verify_code_id, const std::string &verify_code) {
    auto saved = co_await sessions_.GetVerifyCodeCoro(verify_code_id);
    if (!saved.ok()) {
        co_return Result<std::string>::Fail(saved.error());
    }
    if (!saved.value().has_value()) {
        co_return Result<std::string>::Fail(user_errors::VerifyCodeExpired());
    }
    const std::string &phone = saved.value().value();
    auto checked = co_await sms_.CheckVerificationCode(phone, verify_code);
    if (!checked.ok()) {
        co_return Result<std::string>::Fail(checked.error());
    }
    co_return Result<std::string>::Ok(phone);
}

bool UserApplicationService::IsValidPhone(const std::string &phone) const {
    return phone.size() == 11 && phone[0] == '1' &&
           std::all_of(phone.begin(), phone.end(),
                       [](unsigned char c) { return std::isdigit(c) != 0; });
}

bool UserApplicationService::IsValidPassword(
    const std::string &password) const {
    if (password.size() < 8 || password.size() > 32) {
        return false;
    }
    bool has_upper = false;
    bool has_lower = false;
    bool has_digit = false;
    for (unsigned char c : password) {
        if (!std::isalnum(c) && c != '_' && c != '-') {
            return false;
        }
        if (std::isupper(c))
            has_upper = true;
        if (std::islower(c))
            has_lower = true;
        if (std::isdigit(c))
            has_digit = true;
    }
    return has_upper && has_lower && has_digit;
}

drogon::Task<VoidResult>
UserApplicationService::IndexUserCoro(const UserRecord &user) {
    co_return co_await search_index_.IndexUserCoro(user);
}

drogon::Task<VoidResult>
UserApplicationService::IndexUserByIdCoro(const std::string &user_id) {
    auto user = co_await users_.FindUserByIdCoro(user_id);
    if (!user.ok()) {
        co_return VoidResult::Fail(user.error());
    }
    if (!user.value().has_value()) {
        co_return VoidResult::Fail(user_errors::UserNotFound());
    }
    co_return co_await IndexUserCoro(user.value().value());
}

drogon::Task<Result<std::string>>
UserApplicationService::LoginUserCoro(const std::string &user_id) {
    const std::string session_id = NewId();
    auto saved = co_await sessions_.SaveSessionCoro(session_id, user_id);
    if (!saved.ok()) {
        co_return Result<std::string>::Fail(saved.error());
    }
    auto online_set = co_await sessions_.SetOnlineIfAbsentCoro(user_id);
    if (!online_set.ok()) {
        co_await sessions_.RemoveSessionCoro(session_id);
        co_return Result<std::string>::Fail(online_set.error());
    }
    if (!online_set.value()) {
        co_await sessions_.RemoveSessionCoro(session_id);
        co_return Result<std::string>::Fail(user_errors::AlreadyLoggedIn());
    }
    co_return Result<std::string>::Ok(session_id);
}

drogon::Task<std::string>
UserApplicationService::GetAvatarContentCoro(const std::string &avatar_id) {
    if (avatar_id.empty()) {
        co_return std::string();
    }
    auto file = co_await clients_.GetFileCoro(avatar_id);
    if (!file.ok() || !file.value().has_value()) {
        co_return std::string();
    }
    co_return file.value()->file_content;
}

drogon::Task<Result<std::string>> UserApplicationService::PutAvatarContentCoro(
    const std::string &avatar_content) {
    co_return co_await clients_.PutFileCoro("avatar", avatar_content);
}

} // namespace zchat
