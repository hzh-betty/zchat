#ifndef ZCHAT_SERVER_SRC_USER_USER_GRPC_SERVICE_H_
#define ZCHAT_SERVER_SRC_USER_USER_GRPC_SERVICE_H_

#include "common/noncopyable.h"

#include <memory>

#include <grpcpp/grpcpp.h>

#include "user.grpc.pb.h"
#include "user/user_service.h"

namespace zchat {

class UserGrpcService final : public zchat::UserService::Service,
                              public NonCopyable {
  public:
    explicit UserGrpcService(std::shared_ptr<UserApplicationService> service);

    ~UserGrpcService() override = default;

    grpc::Status UserRegister(grpc::ServerContext *context,
                              const zchat::UserRegisterReq *request,
                              zchat::UserRegisterRsp *response) override;
    grpc::Status UserLogin(grpc::ServerContext *context,
                           const zchat::UserLoginReq *request,
                           zchat::UserLoginRsp *response) override;
    grpc::Status
    GetPhoneVerifyCode(grpc::ServerContext *context,
                       const zchat::PhoneVerifyCodeReq *request,
                       zchat::PhoneVerifyCodeRsp *response) override;
    grpc::Status PhoneRegister(grpc::ServerContext *context,
                               const zchat::PhoneRegisterReq *request,
                               zchat::PhoneRegisterRsp *response) override;
    grpc::Status PhoneLogin(grpc::ServerContext *context,
                            const zchat::PhoneLoginReq *request,
                            zchat::PhoneLoginRsp *response) override;
    grpc::Status GetUserInfo(grpc::ServerContext *context,
                             const zchat::GetUserInfoReq *request,
                             zchat::GetUserInfoRsp *response) override;
    grpc::Status
    GetMultiUserInfo(grpc::ServerContext *context,
                     const zchat::GetMultiUserInfoReq *request,
                     zchat::GetMultiUserInfoRsp *response) override;
    grpc::Status SetUserAvatar(grpc::ServerContext *context,
                               const zchat::SetUserAvatarReq *request,
                               zchat::SetUserAvatarRsp *response) override;
    grpc::Status SetUserNickname(grpc::ServerContext *context,
                                 const zchat::SetUserNicknameReq *request,
                                 zchat::SetUserNicknameRsp *response) override;
    grpc::Status
    SetUserDescription(grpc::ServerContext *context,
                       const zchat::SetUserDescriptionReq *request,
                       zchat::SetUserDescriptionRsp *response) override;
    grpc::Status
    SetUserPhoneNumber(grpc::ServerContext *context,
                       const zchat::SetUserPhoneNumberReq *request,
                       zchat::SetUserPhoneNumberRsp *response) override;

  private:
    std::shared_ptr<UserApplicationService> service_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_USER_USER_GRPC_SERVICE_H_
