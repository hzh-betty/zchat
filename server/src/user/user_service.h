#ifndef ZCHAT_SERVER_SRC_USER_USER_SERVICE_H_
#define ZCHAT_SERVER_SRC_USER_USER_SERVICE_H_

#include "common/noncopyable.h"

#include <string>

#include <drogon/utils/coroutine.h>

#include "common/search/user_search_index.h"
#include "common/service_clients.h"
#include "common/session_store.h"
#include "user.pb.h"
#include "user/sms_client.h"
#include "user/user_repository.h"

namespace zchat {

class UserApplicationService : public NonCopyable {
  public:
    UserApplicationService(UserRepository &users, ServiceClients &clients,
                           SmsClient &sms, SessionStore &sessions,
                           UserSearchIndex &search_index);

    ~UserApplicationService() = default;

    drogon::Task<zchat::UserRegisterRsp>
    RegisterByNicknameCoro(const zchat::UserRegisterReq &request);
    drogon::Task<zchat::UserLoginRsp>
    LoginByNicknameCoro(const zchat::UserLoginReq &request);
    drogon::Task<zchat::PhoneVerifyCodeRsp>
    GetPhoneVerifyCodeCoro(const zchat::PhoneVerifyCodeReq &request);
    drogon::Task<zchat::PhoneRegisterRsp>
    RegisterByPhoneCoro(const zchat::PhoneRegisterReq &request);
    drogon::Task<zchat::PhoneLoginRsp>
    LoginByPhoneCoro(const zchat::PhoneLoginReq &request);
    drogon::Task<zchat::GetUserInfoRsp>
    GetUserInfoCoro(const zchat::GetUserInfoReq &request);
    drogon::Task<zchat::GetMultiUserInfoRsp>
    GetMultiUserInfoCoro(const zchat::GetMultiUserInfoReq &request);
    drogon::Task<zchat::SearchUsersRsp>
    SearchUsersCoro(const zchat::SearchUsersReq &request);
    drogon::Task<zchat::SetUserAvatarRsp>
    SetAvatarCoro(const zchat::SetUserAvatarReq &request);
    drogon::Task<zchat::SetUserNicknameRsp>
    SetNicknameCoro(const zchat::SetUserNicknameReq &request);
    drogon::Task<zchat::SetUserDescriptionRsp>
    SetDescriptionCoro(const zchat::SetUserDescriptionReq &request);
    drogon::Task<zchat::SetUserPhoneNumberRsp>
    SetPhoneCoro(const zchat::SetUserPhoneNumberReq &request);

    drogon::Task<Result<std::string>>
    UserIdFromSessionCoro(const std::string &session_id);

  private:
    drogon::Task<Result<std::string>>
    ValidateVerifyCodeCoro(const std::string &verify_code_id,
                           const std::string &verify_code);
    bool IsValidPhone(const std::string &phone) const;
    bool IsValidPassword(const std::string &password) const;
    drogon::Task<VoidResult> IndexUserCoro(const UserRecord &user);
    drogon::Task<VoidResult> IndexUserByIdCoro(const std::string &user_id);
    drogon::Task<Result<std::string>> LoginUserCoro(const std::string &user_id);

    drogon::Task<std::string>
    GetAvatarContentCoro(const std::string &avatar_id);
    drogon::Task<Result<std::string>>
    PutAvatarContentCoro(const std::string &avatar_content);

    UserRepository &users_;
    ServiceClients &clients_;
    SmsClient &sms_;
    SessionStore &sessions_;
    UserSearchIndex &search_index_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_USER_USER_SERVICE_H_