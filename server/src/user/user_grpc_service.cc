#include "user/user_grpc_service.h"

#include <utility>

namespace zchat {

UserGrpcService::UserGrpcService(
    std::shared_ptr<UserApplicationService> service)
    : service_(std::move(service)) {}

grpc::Status
UserGrpcService::UserRegister(grpc::ServerContext *,
                              const zchat::UserRegisterReq *request,
                              zchat::UserRegisterRsp *response) {
    *response = service_->RegisterByNickname(*request);
    return grpc::Status::OK;
}

grpc::Status UserGrpcService::UserLogin(grpc::ServerContext *,
                                        const zchat::UserLoginReq *request,
                                        zchat::UserLoginRsp *response) {
    *response = service_->LoginByNickname(*request);
    return grpc::Status::OK;
}

grpc::Status
UserGrpcService::GetPhoneVerifyCode(grpc::ServerContext *,
                                    const zchat::PhoneVerifyCodeReq *request,
                                    zchat::PhoneVerifyCodeRsp *response) {
    *response = service_->GetPhoneVerifyCode(*request);
    return grpc::Status::OK;
}

grpc::Status
UserGrpcService::PhoneRegister(grpc::ServerContext *,
                               const zchat::PhoneRegisterReq *request,
                               zchat::PhoneRegisterRsp *response) {
    *response = service_->RegisterByPhone(*request);
    return grpc::Status::OK;
}

grpc::Status UserGrpcService::PhoneLogin(grpc::ServerContext *,
                                         const zchat::PhoneLoginReq *request,
                                         zchat::PhoneLoginRsp *response) {
    *response = service_->LoginByPhone(*request);
    return grpc::Status::OK;
}

grpc::Status UserGrpcService::GetUserInfo(grpc::ServerContext *,
                                          const zchat::GetUserInfoReq *request,
                                          zchat::GetUserInfoRsp *response) {
    *response = service_->GetUserInfo(*request);
    return grpc::Status::OK;
}

grpc::Status
UserGrpcService::GetMultiUserInfo(grpc::ServerContext *,
                                  const zchat::GetMultiUserInfoReq *request,
                                  zchat::GetMultiUserInfoRsp *response) {
    *response = service_->GetMultiUserInfo(*request);
    return grpc::Status::OK;
}

grpc::Status
UserGrpcService::SetUserAvatar(grpc::ServerContext *,
                               const zchat::SetUserAvatarReq *request,
                               zchat::SetUserAvatarRsp *response) {
    *response = service_->SetAvatar(*request);
    return grpc::Status::OK;
}

grpc::Status
UserGrpcService::SetUserNickname(grpc::ServerContext *,
                                 const zchat::SetUserNicknameReq *request,
                                 zchat::SetUserNicknameRsp *response) {
    *response = service_->SetNickname(*request);
    return grpc::Status::OK;
}

grpc::Status
UserGrpcService::SetUserDescription(grpc::ServerContext *,
                                    const zchat::SetUserDescriptionReq *request,
                                    zchat::SetUserDescriptionRsp *response) {
    *response = service_->SetDescription(*request);
    return grpc::Status::OK;
}

grpc::Status
UserGrpcService::SetUserPhoneNumber(grpc::ServerContext *,
                                    const zchat::SetUserPhoneNumberReq *request,
                                    zchat::SetUserPhoneNumberRsp *response) {
    *response = service_->SetPhone(*request);
    return grpc::Status::OK;
}

} // namespace zchat
