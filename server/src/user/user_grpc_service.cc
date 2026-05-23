#include "user/user_grpc_service.h"

#include <utility>

#include "common/logger.h"

namespace zchat {

UserGrpcService::UserGrpcService(
    std::shared_ptr<UserApplicationService> service)
    : service_(std::move(service)) {}

grpc::Status
UserGrpcService::UserRegister(grpc::ServerContext *,
                              const zchat::UserRegisterReq *request,
                              zchat::UserRegisterRsp *response) {
    ZCHAT_LOG_INFO("UserService::UserRegister request_id={}", request->request_id());
    *response = service_->RegisterByNickname(*request);
    if (!response->success()) {
        ZCHAT_LOG_WARN("UserService::UserRegister failed: request_id={} errmsg={}", request->request_id(), response->errmsg());
    }
    return grpc::Status::OK;
}

grpc::Status UserGrpcService::UserLogin(grpc::ServerContext *,
                                        const zchat::UserLoginReq *request,
                                        zchat::UserLoginRsp *response) {
    ZCHAT_LOG_INFO("UserService::UserLogin request_id={}", request->request_id());
    *response = service_->LoginByNickname(*request);
    if (!response->success()) {
        ZCHAT_LOG_WARN("UserService::UserLogin failed: request_id={} errmsg={}", request->request_id(), response->errmsg());
    }
    return grpc::Status::OK;
}

grpc::Status
UserGrpcService::GetPhoneVerifyCode(grpc::ServerContext *,
                                    const zchat::PhoneVerifyCodeReq *request,
                                    zchat::PhoneVerifyCodeRsp *response) {
    ZCHAT_LOG_INFO("UserService::GetPhoneVerifyCode request_id={}", request->request_id());
    *response = service_->GetPhoneVerifyCode(*request);
    if (!response->success()) {
        ZCHAT_LOG_WARN("UserService::GetPhoneVerifyCode failed: request_id={} errmsg={}", request->request_id(), response->errmsg());
    }
    return grpc::Status::OK;
}

grpc::Status
UserGrpcService::PhoneRegister(grpc::ServerContext *,
                               const zchat::PhoneRegisterReq *request,
                               zchat::PhoneRegisterRsp *response) {
    ZCHAT_LOG_INFO("UserService::PhoneRegister request_id={}", request->request_id());
    *response = service_->RegisterByPhone(*request);
    if (!response->success()) {
        ZCHAT_LOG_WARN("UserService::PhoneRegister failed: request_id={} errmsg={}", request->request_id(), response->errmsg());
    }
    return grpc::Status::OK;
}

grpc::Status UserGrpcService::PhoneLogin(grpc::ServerContext *,
                                         const zchat::PhoneLoginReq *request,
                                         zchat::PhoneLoginRsp *response) {
    ZCHAT_LOG_INFO("UserService::PhoneLogin request_id={}", request->request_id());
    *response = service_->LoginByPhone(*request);
    if (!response->success()) {
        ZCHAT_LOG_WARN("UserService::PhoneLogin failed: request_id={} errmsg={}", request->request_id(), response->errmsg());
    }
    return grpc::Status::OK;
}

grpc::Status UserGrpcService::GetUserInfo(grpc::ServerContext *,
                                          const zchat::GetUserInfoReq *request,
                                          zchat::GetUserInfoRsp *response) {
    ZCHAT_LOG_INFO("UserService::GetUserInfo request_id={}", request->request_id());
    *response = service_->GetUserInfo(*request);
    if (!response->success()) {
        ZCHAT_LOG_WARN("UserService::GetUserInfo failed: request_id={} errmsg={}", request->request_id(), response->errmsg());
    }
    return grpc::Status::OK;
}

grpc::Status
UserGrpcService::GetMultiUserInfo(grpc::ServerContext *,
                                  const zchat::GetMultiUserInfoReq *request,
                                  zchat::GetMultiUserInfoRsp *response) {
    ZCHAT_LOG_INFO("UserService::GetMultiUserInfo request_id={}", request->request_id());
    *response = service_->GetMultiUserInfo(*request);
    if (!response->success()) {
        ZCHAT_LOG_WARN("UserService::GetMultiUserInfo failed: request_id={} errmsg={}", request->request_id(), response->errmsg());
    }
    return grpc::Status::OK;
}

grpc::Status
UserGrpcService::SetUserAvatar(grpc::ServerContext *,
                               const zchat::SetUserAvatarReq *request,
                               zchat::SetUserAvatarRsp *response) {
    ZCHAT_LOG_INFO("UserService::SetUserAvatar request_id={}", request->request_id());
    *response = service_->SetAvatar(*request);
    if (!response->success()) {
        ZCHAT_LOG_WARN("UserService::SetUserAvatar failed: request_id={} errmsg={}", request->request_id(), response->errmsg());
    }
    return grpc::Status::OK;
}

grpc::Status
UserGrpcService::SetUserNickname(grpc::ServerContext *,
                                 const zchat::SetUserNicknameReq *request,
                                 zchat::SetUserNicknameRsp *response) {
    ZCHAT_LOG_INFO("UserService::SetUserNickname request_id={}", request->request_id());
    *response = service_->SetNickname(*request);
    if (!response->success()) {
        ZCHAT_LOG_WARN("UserService::SetUserNickname failed: request_id={} errmsg={}", request->request_id(), response->errmsg());
    }
    return grpc::Status::OK;
}

grpc::Status
UserGrpcService::SetUserDescription(grpc::ServerContext *,
                                    const zchat::SetUserDescriptionReq *request,
                                    zchat::SetUserDescriptionRsp *response) {
    ZCHAT_LOG_INFO("UserService::SetUserDescription request_id={}", request->request_id());
    *response = service_->SetDescription(*request);
    if (!response->success()) {
        ZCHAT_LOG_WARN("UserService::SetUserDescription failed: request_id={} errmsg={}", request->request_id(), response->errmsg());
    }
    return grpc::Status::OK;
}

grpc::Status
UserGrpcService::SetUserPhoneNumber(grpc::ServerContext *,
                                    const zchat::SetUserPhoneNumberReq *request,
                                    zchat::SetUserPhoneNumberRsp *response) {
    ZCHAT_LOG_INFO("UserService::SetUserPhoneNumber request_id={}", request->request_id());
    *response = service_->SetPhone(*request);
    if (!response->success()) {
        ZCHAT_LOG_WARN("UserService::SetUserPhoneNumber failed: request_id={} errmsg={}", request->request_id(), response->errmsg());
    }
    return grpc::Status::OK;
}

} // namespace zchat
