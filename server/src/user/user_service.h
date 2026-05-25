#ifndef ZCHAT_SERVER_SRC_USER_USER_SERVICE_H_
#define ZCHAT_SERVER_SRC_USER_USER_SERVICE_H_

#include "common/noncopyable.h"

#include <string>

#include "common/session_store.h"
#include "file.pb.h"
#include "file/file_repository.h"
#include "user.pb.h"
#include "user/sms_client.h"
#include "user/user_repository.h"
#include "user/user_search_index.h"

namespace zchat {

class UserApplicationService : public NonCopyable {
  public:
    UserApplicationService(UserRepository &users, FileRepository &files,
                           SmsClient &sms, SessionStore &sessions,
                           UserSearchIndex &search_index);

    ~UserApplicationService() = default;

    zchat::UserRegisterRsp
    RegisterByNickname(const zchat::UserRegisterReq &request);
    zchat::UserLoginRsp LoginByNickname(const zchat::UserLoginReq &request);
    zchat::PhoneVerifyCodeRsp
    GetPhoneVerifyCode(const zchat::PhoneVerifyCodeReq &request);
    zchat::PhoneRegisterRsp
    RegisterByPhone(const zchat::PhoneRegisterReq &request);
    zchat::PhoneLoginRsp LoginByPhone(const zchat::PhoneLoginReq &request);
    zchat::GetUserInfoRsp GetUserInfo(const zchat::GetUserInfoReq &request);
    zchat::GetMultiUserInfoRsp
    GetMultiUserInfo(const zchat::GetMultiUserInfoReq &request);
    zchat::SetUserAvatarRsp SetAvatar(const zchat::SetUserAvatarReq &request);
    zchat::SetUserNicknameRsp
    SetNickname(const zchat::SetUserNicknameReq &request);
    zchat::SetUserDescriptionRsp
    SetDescription(const zchat::SetUserDescriptionReq &request);
    zchat::SetUserPhoneNumberRsp
    SetPhone(const zchat::SetUserPhoneNumberReq &request);

    Result<std::string> UserIdFromSession(const std::string &session_id);

  private:
    Result<std::string> ValidateVerifyCode(const std::string &verify_code_id,
                                           const std::string &verify_code);
    bool IsValidPhone(const std::string &phone) const;
    bool IsValidPassword(const std::string &password) const;
    void IndexUser(const UserRecord &user);
    void IndexUserById(const std::string &user_id);
    std::string LoginUser(const std::string &user_id);

    UserRepository &users_;
    FileRepository &files_;
    SmsClient &sms_;
    SessionStore &sessions_;
    UserSearchIndex &search_index_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_USER_USER_SERVICE_H_
