#include "user/user_service.h"

#include <algorithm>
#include <cctype>
#include <vector>

#include "common/logger.h"
#include "common/proto_mapper.h"
#include "common/uuid.h"

namespace zchat {
namespace {

template <typename Response>
Response ErrorResponse(const std::string &request_id,
                       const std::string &message) {
    Response response;
    response.set_request_id(request_id);
    response.set_success(false);
    response.set_errmsg(message);
    return response;
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
        return ErrorResponse<zchat::UserRegisterRsp>(request.request_id(),
                                                     "用户名不能为空");
    }
    if (!IsValidPassword(request.password())) {
        return ErrorResponse<zchat::UserRegisterRsp>(request.request_id(),
                                                     "密码格式不合法");
    }
    auto existing = users_.FindUserByNickname(request.nickname());
    if (!existing.ok()) {
        return ErrorResponse<zchat::UserRegisterRsp>(request.request_id(),
                                                     existing.error().message);
    }
    if (existing.value().has_value()) {
        return ErrorResponse<zchat::UserRegisterRsp>(request.request_id(),
                                                     "用户名被占用");
    }

    UserRecord user;
    user.user_id = NewId();
    user.nickname = request.nickname();
    user.password = request.password();
    const auto inserted = users_.InsertUser(user);
    if (!inserted.ok()) {
        ZCHAT_LOG_ERROR("RegisterByNickname db failed: {}", inserted.error().message);
        return ErrorResponse<zchat::UserRegisterRsp>(request.request_id(),
                                                     inserted.error().message);
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
                                                  user.error().message);
    }
    if (!user.value().has_value() ||
        user.value()->password != request.password()) {
        ZCHAT_LOG_WARN("LoginByNickname rejected: request_id={} reason={}", request.request_id(), "用户名或密码错误");
        return ErrorResponse<zchat::UserLoginRsp>(request.request_id(),
                                                  "用户名或密码错误");
    }

    auto session_id = LoginUser(user.value()->user_id);
    if (!session_id.ok()) {
        ZCHAT_LOG_WARN("LoginByNickname rejected: request_id={} reason={}",
                       request.request_id(), session_id.error().message);
        return ErrorResponse<zchat::UserLoginRsp>(request.request_id(),
                                                  session_id.error().message);
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
        return ErrorResponse<zchat::PhoneVerifyCodeRsp>(request.request_id(),
                                                         "手机号码格式错误");
    }
    const std::string verify_code_id = NewId();
    const auto saved =
        sessions_.SaveVerifyCode(verify_code_id, request.phone_number());
    if (!saved.ok()) {
        ZCHAT_LOG_ERROR("GetPhoneVerifyCode redis failed: {}", saved.error().message);
        return ErrorResponse<zchat::PhoneVerifyCodeRsp>(request.request_id(),
                                                         saved.error().message);
    }
    const auto sent = sms_.SendVerificationCode(request.phone_number());
    if (!sent.ok()) {
        ZCHAT_LOG_ERROR("GetPhoneVerifyCode sms failed: {}", sent.error().message);
        sessions_.RemoveVerifyCode(verify_code_id);
        return ErrorResponse<zchat::PhoneVerifyCodeRsp>(request.request_id(),
                                                         sent.error().message);
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
        return ErrorResponse<zchat::PhoneRegisterRsp>(request.request_id(),
                                                      "手机号码格式错误");
    }
    auto existing_user = users_.FindUserByPhone(request.phone_number());
    if (!existing_user.ok()) {
        return ErrorResponse<zchat::PhoneRegisterRsp>(
            request.request_id(), existing_user.error().message);
    }
    if (existing_user.value().has_value()) {
        return ErrorResponse<zchat::PhoneRegisterRsp>(request.request_id(),
                                                      "手机号已注册");
    }
    const auto code =
        ValidateVerifyCode(request.verify_code_id(), request.verify_code());
    if (!code.ok()) {
        ZCHAT_LOG_WARN("RegisterByPhone rejected: request_id={} reason={}", request.request_id(), "验证码失败");
        return ErrorResponse<zchat::PhoneRegisterRsp>(request.request_id(),
                                                      code.error().message);
    }
    if (code.value() != request.phone_number()) {
        ZCHAT_LOG_WARN("RegisterByPhone rejected: request_id={} reason=phone mismatch", request.request_id());
        return ErrorResponse<zchat::PhoneRegisterRsp>(request.request_id(),
                                                      "验证码与手机号不匹配");
    }
    auto existing = users_.FindUserByPhone(request.phone_number());
    if (!existing.ok()) {
        return ErrorResponse<zchat::PhoneRegisterRsp>(request.request_id(),
                                                      existing.error().message);
    }
    if (existing.value().has_value()) {
        return ErrorResponse<zchat::PhoneRegisterRsp>(request.request_id(),
                                                      "该手机号已注册");
    }
    UserRecord user;
    user.user_id = NewId();
    user.nickname = user.user_id;
    user.phone = request.phone_number();
    const auto inserted = users_.InsertUser(user);
    if (!inserted.ok()) {
        ZCHAT_LOG_ERROR("RegisterByPhone db failed: {}", inserted.error().message);
        return ErrorResponse<zchat::PhoneRegisterRsp>(request.request_id(),
                                                      inserted.error().message);
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
        return ErrorResponse<zchat::PhoneLoginRsp>(request.request_id(),
                                                   "手机号码格式错误");
    }
    auto existing_user = users_.FindUserByPhone(request.phone_number());
    if (!existing_user.ok()) {
        return ErrorResponse<zchat::PhoneLoginRsp>(
            request.request_id(), existing_user.error().message);
    }
    if (!existing_user.value().has_value()) {
        return ErrorResponse<zchat::PhoneLoginRsp>(request.request_id(),
                                                   "手机号未注册");
    }
    const auto code =
        ValidateVerifyCode(request.verify_code_id(), request.verify_code());
    if (!code.ok()) {
        ZCHAT_LOG_WARN("LoginByPhone rejected: request_id={} reason={}", request.request_id(), "验证码失败");
        return ErrorResponse<zchat::PhoneLoginRsp>(request.request_id(),
                                                   code.error().message);
    }
    if (code.value() != request.phone_number()) {
        ZCHAT_LOG_WARN("LoginByPhone rejected: request_id={} reason=phone mismatch", request.request_id());
        return ErrorResponse<zchat::PhoneLoginRsp>(request.request_id(),
                                                   "验证码与手机号不匹配");
    }
    auto user = users_.FindUserByPhone(request.phone_number());
    if (!user.ok()) {
        return ErrorResponse<zchat::PhoneLoginRsp>(request.request_id(),
                                                   user.error().message);
    }
    if (!user.value().has_value()) {
        return ErrorResponse<zchat::PhoneLoginRsp>(request.request_id(),
                                                   "手机号未注册");
    }
    auto session_id = LoginUser(user.value()->user_id);
    if (!session_id.ok()) {
        ZCHAT_LOG_WARN("LoginByPhone rejected: request_id={} reason={}",
                       request.request_id(), session_id.error().message);
        return ErrorResponse<zchat::PhoneLoginRsp>(request.request_id(),
                                                   session_id.error().message);
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
                                                    user_id.error().message);
    }
    auto user = users_.FindUserById(user_id.value());
    if (!user.ok()) {
        ZCHAT_LOG_ERROR("GetUserInfo db failed: {}", user.error().message);
        return ErrorResponse<zchat::GetUserInfoRsp>(request.request_id(),
                                                    user.error().message);
    }
    if (!user.value().has_value()) {
        return ErrorResponse<zchat::GetUserInfoRsp>(request.request_id(),
                                                    "用户不存在");
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
        response.set_errmsg(users.error().message);
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
                                                      user_id.error().message);
    }
    const std::string file_id = NewId();
    const auto file_result = files_.PutFile(FileRecord{
        file_id, "avatar", request.avatar().size(), request.avatar()});
    if (!file_result.ok()) {
        ZCHAT_LOG_ERROR("SetAvatar db failed: {}", file_result.error().message);
        return ErrorResponse<zchat::SetUserAvatarRsp>(
            request.request_id(), file_result.error().message);
    }
    const auto updated = users_.UpdateUserAvatar(user_id.value(), file_id);
    if (!updated.ok()) {
        ZCHAT_LOG_ERROR("SetAvatar db failed: {}", updated.error().message);
        return ErrorResponse<zchat::SetUserAvatarRsp>(request.request_id(),
                                                      updated.error().message);
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
            request.request_id(), user_id.error().message);
    }
    const auto updated =
        users_.UpdateUserNickname(user_id.value(), request.nickname());
    if (!updated.ok()) {
        ZCHAT_LOG_ERROR("SetNickname db failed: {}", updated.error().message);
        return ErrorResponse<zchat::SetUserNicknameRsp>(
            request.request_id(), updated.error().message);
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
            request.request_id(), user_id.error().message);
    }
    const auto updated =
        users_.UpdateUserDescription(user_id.value(), request.description());
    if (!updated.ok()) {
        ZCHAT_LOG_ERROR("SetDescription db failed: {}", updated.error().message);
        return ErrorResponse<zchat::SetUserDescriptionRsp>(
            request.request_id(), updated.error().message);
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
            request.request_id(), user_id.error().message);
    }
    const auto code = ValidateVerifyCode(request.phone_verify_code_id(),
                                         request.phone_verify_code());
    if (!code.ok()) {
        ZCHAT_LOG_WARN("SetPhone rejected: request_id={} reason={}", request.request_id(), "验证码失败");
        return ErrorResponse<zchat::SetUserPhoneNumberRsp>(
            request.request_id(), code.error().message);
    }
    if (code.value() != request.phone_number()) {
        ZCHAT_LOG_WARN("SetPhone rejected: request_id={} reason=phone mismatch", request.request_id());
        return ErrorResponse<zchat::SetUserPhoneNumberRsp>(
            request.request_id(), "验证码与手机号不匹配");
    }
    const auto updated =
        users_.UpdateUserPhone(user_id.value(), request.phone_number());
    if (!updated.ok()) {
        ZCHAT_LOG_ERROR("SetPhone db failed: {}", updated.error().message);
        return ErrorResponse<zchat::SetUserPhoneNumberRsp>(
            request.request_id(), updated.error().message);
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
        ZCHAT_LOG_WARN("session invalid: session_id is empty");
        return Result<std::string>::Fail("登录会话为空");
    }
    auto user_id = sessions_.GetUserId(session_id);
    if (!user_id.ok()) {
        ZCHAT_LOG_ERROR("UserIdFromSession redis failed: {}", user_id.error().message);
        return Result<std::string>::Fail(user_id.error().message);
    }
    if (!user_id.value().has_value()) {
        ZCHAT_LOG_WARN("session invalid: session_id={}", session_id);
        return Result<std::string>::Fail("登录会话已失效");
    }
    return Result<std::string>::Ok(user_id.value().value());
}

Result<std::string>
UserApplicationService::ValidateVerifyCode(const std::string &verify_code_id,
                                           const std::string &verify_code) {
    auto saved = sessions_.GetVerifyCode(verify_code_id);
    if (!saved.ok()) {
        return Result<std::string>::Fail(saved.error().message);
    }
    if (!saved.value().has_value()) {
        return Result<std::string>::Fail("验证码已失效");
    }
    const std::string &phone = saved.value().value();
    auto checked = sms_.CheckVerificationCode(phone, verify_code);
    if (!checked.ok()) {
        return Result<std::string>::Fail(checked.error().message);
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
        return Result<std::string>::Fail(online.error().message);
    }
    if (online.value()) {
        return Result<std::string>::Fail("用户已在其他地方登录");
    }
    const std::string session_id = NewId();
    auto saved = sessions_.SaveSession(session_id, user_id);
    if (!saved.ok()) {
        return Result<std::string>::Fail(saved.error().message);
    }
    auto online_set = sessions_.SetOnline(user_id);
    if (!online_set.ok()) {
        sessions_.RemoveSession(session_id);
        return Result<std::string>::Fail(online_set.error().message);
    }
    return Result<std::string>::Ok(session_id);
}

} // namespace zchat
