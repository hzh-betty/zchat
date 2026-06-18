#ifndef ZCHAT_SERVER_SRC_USER_USER_GRPC_SERVICE_H_
#define ZCHAT_SERVER_SRC_USER_USER_GRPC_SERVICE_H_

#include "common/noncopyable.h"

#include <memory>

#include <grpcpp/grpcpp.h>

#include "user.grpc.pb.h"
#include "user/user_service.h"

namespace zchat {

class UserGrpcService final : public zchat::UserService::CallbackService,
                              public NonCopyable {
  public:
    explicit UserGrpcService(std::shared_ptr<UserApplicationService> service);

    ~UserGrpcService() override = default;

    grpc::ServerUnaryReactor *
    UserRegister(grpc::CallbackServerContext *context,
                 const zchat::UserRegisterReq *request,
                 zchat::UserRegisterRsp *response) override;
    grpc::ServerUnaryReactor *UserLogin(grpc::CallbackServerContext *context,
                                        const zchat::UserLoginReq *request,
                                        zchat::UserLoginRsp *response) override;
    grpc::ServerUnaryReactor *
    GetPhoneVerifyCode(grpc::CallbackServerContext *context,
                       const zchat::PhoneVerifyCodeReq *request,
                       zchat::PhoneVerifyCodeRsp *response) override;
    grpc::ServerUnaryReactor *
    PhoneRegister(grpc::CallbackServerContext *context,
                  const zchat::PhoneRegisterReq *request,
                  zchat::PhoneRegisterRsp *response) override;
    grpc::ServerUnaryReactor *
    PhoneLogin(grpc::CallbackServerContext *context,
               const zchat::PhoneLoginReq *request,
               zchat::PhoneLoginRsp *response) override;
    grpc::ServerUnaryReactor *
    GetUserInfo(grpc::CallbackServerContext *context,
                const zchat::GetUserInfoReq *request,
                zchat::GetUserInfoRsp *response) override;
    grpc::ServerUnaryReactor *
    GetMultiUserInfo(grpc::CallbackServerContext *context,
                     const zchat::GetMultiUserInfoReq *request,
                     zchat::GetMultiUserInfoRsp *response) override;
    grpc::ServerUnaryReactor *
    SearchUsers(grpc::CallbackServerContext *context,
                const zchat::SearchUsersReq *request,
                zchat::SearchUsersRsp *response) override;
    grpc::ServerUnaryReactor *
    SetUserAvatar(grpc::CallbackServerContext *context,
                  const zchat::SetUserAvatarReq *request,
                  zchat::SetUserAvatarRsp *response) override;
    grpc::ServerUnaryReactor *
    SetUserNickname(grpc::CallbackServerContext *context,
                    const zchat::SetUserNicknameReq *request,
                    zchat::SetUserNicknameRsp *response) override;
    grpc::ServerUnaryReactor *
    SetUserDescription(grpc::CallbackServerContext *context,
                       const zchat::SetUserDescriptionReq *request,
                       zchat::SetUserDescriptionRsp *response) override;
    grpc::ServerUnaryReactor *
    SetUserPhoneNumber(grpc::CallbackServerContext *context,
                       const zchat::SetUserPhoneNumberReq *request,
                       zchat::SetUserPhoneNumberRsp *response) override;

  private:
    std::shared_ptr<UserApplicationService> service_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_USER_USER_GRPC_SERVICE_H_
