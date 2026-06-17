#include "user/user_service.h"

#include <algorithm>
#include <cctype>
#include <vector>

#include "common/common_errors.h"
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

zchat::UserRegisterRsp UserApplicationService::RegisterByNickname(
    const zchat::UserRegisterReq &request) {
    ZCHAT_LOG_INFO("RegisterByNickname request_id={}", request.request_id());
    if (request.nickname().empty()) {
        return ErrorResponse<zchat::UserRegisterRsp>(
            request.request_id(), user_errors::NicknameRequired());
    }
    if (!IsValidPassword(request.password())) {
        return ErrorResponse<zchat::UserRegisterRsp>(
            request.request_id(), user_errors::InvalidPassword());
    }
    auto existing = users_.FindUserByNickname(request.nickname());
    if (!existing.ok()) {
        return ErrorResponse<zchat::UserRegisterRsp>(request.request_id(),
                                                     existing.error());
    }
    if (existing.value().has_value()) {
        return ErrorResponse<zchat::UserRegisterRsp>(
            request.request_id(), user_errors::NicknameAlreadyExists());
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
    const auto indexed = IndexUser(user);
    if (!indexed.ok()) {
        return ErrorResponse<zchat::UserRegisterRsp>(request.request_id(),
                                                     indexed.error());
    }

    zchat::UserRegisterRsp response;
    MarkOk(request.request_id(), &response);
    ZCHAT_LOG_INFO("RegisterByNickname success: request_id={}",
                   request.request_id());
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
            request.request_id(), user_errors::InvalidCredentials());
    }

    auto session_id = LoginUser(user.value()->user_id);
    if (!session_id.ok()) {
        return ErrorResponse<zchat::UserLoginRsp>(request.request_id(),
                                                  session_id.error());
    }
    zchat::UserLoginRsp response;
    MarkOk(request.request_id(), &response);
    response.set_login_session_id(session_id.value());
    ZCHAT_LOG_INFO("LoginByNickname success: request_id={} user_id={}",
                   request.request_id(), user.value()->user_id);
    return response;
}

zchat::PhoneVerifyCodeRsp UserApplicationService::GetPhoneVerifyCode(
    const zchat::PhoneVerifyCodeReq &request) {
    ZCHAT_LOG_INFO("GetPhoneVerifyCode request_id={}", request.request_id());
    if (!IsValidPhone(request.phone_number())) {
        return ErrorResponse<zchat::PhoneVerifyCodeRsp>(
            request.request_id(), user_errors::InvalidPhone());
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
    ZCHAT_LOG_INFO("GetPhoneVerifyCode success: request_id={} phone={}",
                   request.request_id(), request.phone_number());
    return response;
}

zchat::PhoneRegisterRsp UserApplicationService::RegisterByPhone(
    const zchat::PhoneRegisterReq &request) {
    ZCHAT_LOG_INFO("RegisterByPhone request_id={}", request.request_id());
    if (!IsValidPhone(request.phone_number())) {
        return ErrorResponse<zchat::PhoneRegisterRsp>(
            request.request_id(), user_errors::InvalidPhone());
    }
    auto existing_user = users_.FindUserByPhone(request.phone_number());
    if (!existing_user.ok()) {
        return ErrorResponse<zchat::PhoneRegisterRsp>(request.request_id(),
                                                      existing_user.error());
    }
    if (existing_user.value().has_value()) {
        return ErrorResponse<zchat::PhoneRegisterRsp>(
            request.request_id(), user_errors::PhoneAlreadyRegistered());
    }
    const auto code =
        ValidateVerifyCode(request.verify_code_id(), request.verify_code());
    if (!code.ok()) {
        return ErrorResponse<zchat::PhoneRegisterRsp>(request.request_id(),
                                                      code.error());
    }
    if (code.value() != request.phone_number()) {
        return ErrorResponse<zchat::PhoneRegisterRsp>(
            request.request_id(), user_errors::VerifyCodePhoneMismatch());
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
    const auto indexed = IndexUser(user);
    if (!indexed.ok()) {
        return ErrorResponse<zchat::PhoneRegisterRsp>(request.request_id(),
                                                      indexed.error());
    }
    sessions_.RemoveVerifyCode(request.verify_code_id());
    zchat::PhoneRegisterRsp response;
    MarkOk(request.request_id(), &response);
    ZCHAT_LOG_INFO("RegisterByPhone success: request_id={}",
                   request.request_id());
    return response;
}

zchat::PhoneLoginRsp
UserApplicationService::LoginByPhone(const zchat::PhoneLoginReq &request) {
    ZCHAT_LOG_INFO("LoginByPhone request_id={}", request.request_id());
    if (!IsValidPhone(request.phone_number())) {
        return ErrorResponse<zchat::PhoneLoginRsp>(request.request_id(),
                                                   user_errors::InvalidPhone());
    }
    auto existing_user = users_.FindUserByPhone(request.phone_number());
    if (!existing_user.ok()) {
        return ErrorResponse<zchat::PhoneLoginRsp>(request.request_id(),
                                                   existing_user.error());
    }
    if (!existing_user.value().has_value()) {
        return ErrorResponse<zchat::PhoneLoginRsp>(
            request.request_id(), user_errors::PhoneNotRegistered());
    }
    const auto code =
        ValidateVerifyCode(request.verify_code_id(), request.verify_code());
    if (!code.ok()) {
        return ErrorResponse<zchat::PhoneLoginRsp>(request.request_id(),
                                                   code.error());
    }
    if (code.value() != request.phone_number()) {
        return ErrorResponse<zchat::PhoneLoginRsp>(
            request.request_id(), user_errors::VerifyCodePhoneMismatch());
    }
    auto user = users_.FindUserByPhone(request.phone_number());
    if (!user.ok()) {
        return ErrorResponse<zchat::PhoneLoginRsp>(request.request_id(),
                                                   user.error());
    }
    if (!user.value().has_value()) {
        return ErrorResponse<zchat::PhoneLoginRsp>(
            request.request_id(), user_errors::PhoneNotRegistered());
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
            request.request_id(), user_errors::UserNotFound());
    }
    std::string avatar = GetAvatarContent(user.value()->avatar_id);
    zchat::GetUserInfoRsp response;
    MarkOk(request.request_id(), &response);
    *response.mutable_user_info() = ToProtoUser(user.value().value(), avatar);
    ZCHAT_LOG_INFO("GetUserInfo success: request_id={} user_id={}",
                   request.request_id(), user_id.value());
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
        std::string avatar = GetAvatarContent(user.avatar_id);
        (*response.mutable_users_info())[user.user_id] =
            ToProtoUser(user, avatar);
    }
    return response;
}

zchat::SearchUsersRsp
UserApplicationService::SearchUsers(const zchat::SearchUsersReq &request) {
    ZCHAT_LOG_INFO("SearchUsers request_id={}", request.request_id());
    auto users = search_index_.SearchUsers(
        request.search_key(), {request.exclude_user_id()});
    if (!users.ok()) {
        return ErrorResponse<zchat::SearchUsersRsp>(request.request_id(),
                                                    users.error());
    }
    zchat::SearchUsersRsp response;
    response.set_request_id(request.request_id());
    response.set_success(true);
    response.set_errmsg("");
    for (const auto &user : users.value()) {
        std::string avatar = GetAvatarContent(user.avatar_id);
        *response.add_user_info() = ToProtoUser(user, avatar);
    }
    ZCHAT_LOG_INFO("SearchUsers success: request_id={} count={}",
                   request.request_id(), users.value().size());
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
    const auto file_id = PutAvatarContent(request.avatar());
    if (!file_id.ok()) {
        return ErrorResponse<zchat::SetUserAvatarRsp>(request.request_id(),
                                                      file_id.error());
    }
    const auto updated =
        users_.UpdateUserAvatar(user_id.value(), file_id.value());
    if (!updated.ok()) {
        return ErrorResponse<zchat::SetUserAvatarRsp>(request.request_id(),
                                                      updated.error());
    }
    const auto indexed = IndexUserById(user_id.value());
    if (!indexed.ok()) {
        return ErrorResponse<zchat::SetUserAvatarRsp>(request.request_id(),
                                                      indexed.error());
    }
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
        return ErrorResponse<zchat::SetUserNicknameRsp>(request.request_id(),
                                                        user_id.error());
    }
    const auto updated =
        users_.UpdateUserNickname(user_id.value(), request.nickname());
    if (!updated.ok()) {
        return ErrorResponse<zchat::SetUserNicknameRsp>(request.request_id(),
                                                        updated.error());
    }
    const auto indexed = IndexUserById(user_id.value());
    if (!indexed.ok()) {
        return ErrorResponse<zchat::SetUserNicknameRsp>(request.request_id(),
                                                        indexed.error());
    }
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
        return ErrorResponse<zchat::SetUserDescriptionRsp>(request.request_id(),
                                                           user_id.error());
    }
    const auto updated =
        users_.UpdateUserDescription(user_id.value(), request.description());
    if (!updated.ok()) {
        return ErrorResponse<zchat::SetUserDescriptionRsp>(request.request_id(),
                                                           updated.error());
    }
    const auto indexed = IndexUserById(user_id.value());
    if (!indexed.ok()) {
        return ErrorResponse<zchat::SetUserDescriptionRsp>(request.request_id(),
                                                           indexed.error());
    }
    zchat::SetUserDescriptionRsp response;
    MarkOk(request.request_id(), &response);
    ZCHAT_LOG_INFO("SetDescription success: request_id={}",
                   request.request_id());
    return response;
}

zchat::SetUserPhoneNumberRsp
UserApplicationService::SetPhone(const zchat::SetUserPhoneNumberReq &request) {
    ZCHAT_LOG_INFO("SetPhone request_id={}", request.request_id());
    const auto user_id = UserIdFromSession(request.session_id());
    if (!user_id.ok()) {
        return ErrorResponse<zchat::SetUserPhoneNumberRsp>(request.request_id(),
                                                           user_id.error());
    }
    const auto code = ValidateVerifyCode(request.phone_verify_code_id(),
                                         request.phone_verify_code());
    if (!code.ok()) {
        return ErrorResponse<zchat::SetUserPhoneNumberRsp>(request.request_id(),
                                                           code.error());
    }
    if (code.value() != request.phone_number()) {
        return ErrorResponse<zchat::SetUserPhoneNumberRsp>(
            request.request_id(), user_errors::VerifyCodePhoneMismatch());
    }
    const auto updated =
        users_.UpdateUserPhone(user_id.value(), request.phone_number());
    if (!updated.ok()) {
        return ErrorResponse<zchat::SetUserPhoneNumberRsp>(request.request_id(),
                                                           updated.error());
    }
    const auto indexed = IndexUserById(user_id.value());
    if (!indexed.ok()) {
        return ErrorResponse<zchat::SetUserPhoneNumberRsp>(request.request_id(),
                                                           indexed.error());
    }
    sessions_.RemoveVerifyCode(request.phone_verify_code_id());
    zchat::SetUserPhoneNumberRsp response;
    MarkOk(request.request_id(), &response);
    ZCHAT_LOG_INFO("SetPhone success: request_id={}", request.request_id());
    return response;
}

Result<std::string>
UserApplicationService::UserIdFromSession(const std::string &session_id) {
    if (session_id.empty()) {
        return Result<std::string>::Fail(common_errors::SessionIdRequired());
    }
    auto user_id = sessions_.GetUserId(session_id);
    if (!user_id.ok()) {
        return Result<std::string>::Fail(user_id.error());
    }
    if (!user_id.value().has_value()) {
        return Result<std::string>::Fail(common_errors::SessionExpired());
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
        return Result<std::string>::Fail(user_errors::VerifyCodeExpired());
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

VoidResult UserApplicationService::IndexUser(const UserRecord &user) {
    return search_index_.IndexUser(user);
}

VoidResult UserApplicationService::IndexUserById(const std::string &user_id) {
    auto user = users_.FindUserById(user_id);
    if (!user.ok()) {
        return VoidResult::Fail(user.error());
    }
    if (!user.value().has_value()) {
        return VoidResult::Fail(user_errors::UserNotFound());
    }
    return IndexUser(user.value().value());
}

Result<std::string>
UserApplicationService::LoginUser(const std::string &user_id) {
    auto online = sessions_.IsOnline(user_id);
    if (!online.ok()) {
        return Result<std::string>::Fail(online.error());
    }
    if (online.value()) {
        return Result<std::string>::Fail(user_errors::AlreadyLoggedIn());
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

std::string
UserApplicationService::GetAvatarContent(const std::string &avatar_id) {
    if (avatar_id.empty()) {
        return std::string();
    }
    auto file = clients_.GetFile(avatar_id);
    if (!file.ok() || !file.value().has_value()) {
        return std::string();
    }
    return file.value()->file_content;
}

Result<std::string>
UserApplicationService::PutAvatarContent(const std::string &avatar_content) {
    return clients_.PutFile("avatar", avatar_content);
}

} // namespace zchat