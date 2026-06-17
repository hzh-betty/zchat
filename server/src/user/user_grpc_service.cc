#include "user/user_grpc_service.h"

#include <utility>

#include "common/error_response.h"
#include "common/logger.h"

namespace zchat {

UserGrpcService::UserGrpcService(
    std::shared_ptr<UserApplicationService> service)
    : service_(std::move(service)) {}

grpc::Status
UserGrpcService::UserRegister(grpc::ServerContext *,
                              const zchat::UserRegisterReq *request,
                              zchat::UserRegisterRsp *response) {
    ZCHAT_LOG_INFO("UserService::UserRegister request_id={}",
                   request->request_id());
    *response = service_->RegisterByNickname(*request);
    LogBoundaryResponseError("UserService", "UserRegister",
                             request->request_id(), *response);
    return grpc::Status::OK;
}

grpc::Status UserGrpcService::UserLogin(grpc::ServerContext *,
                                        const zchat::UserLoginReq *request,
                                        zchat::UserLoginRsp *response) {
    ZCHAT_LOG_INFO("UserService::UserLogin request_id={}",
                   request->request_id());
    *response = service_->LoginByNickname(*request);
    LogBoundaryResponseError("UserService", "UserLogin", request->request_id(),
                             *response);
    return grpc::Status::OK;
}

grpc::Status
UserGrpcService::GetPhoneVerifyCode(grpc::ServerContext *,
                                    const zchat::PhoneVerifyCodeReq *request,
                                    zchat::PhoneVerifyCodeRsp *response) {
    ZCHAT_LOG_INFO("UserService::GetPhoneVerifyCode request_id={}",
                   request->request_id());
    *response = service_->GetPhoneVerifyCode(*request);
    LogBoundaryResponseError("UserService", "GetPhoneVerifyCode",
                             request->request_id(), *response);
    return grpc::Status::OK;
}

grpc::Status
UserGrpcService::PhoneRegister(grpc::ServerContext *,
                               const zchat::PhoneRegisterReq *request,
                               zchat::PhoneRegisterRsp *response) {
    ZCHAT_LOG_INFO("UserService::PhoneRegister request_id={}",
                   request->request_id());
    *response = service_->RegisterByPhone(*request);
    LogBoundaryResponseError("UserService", "PhoneRegister",
                             request->request_id(), *response);
    return grpc::Status::OK;
}

grpc::Status UserGrpcService::PhoneLogin(grpc::ServerContext *,
                                         const zchat::PhoneLoginReq *request,
                                         zchat::PhoneLoginRsp *response) {
    ZCHAT_LOG_INFO("UserService::PhoneLogin request_id={}",
                   request->request_id());
    *response = service_->LoginByPhone(*request);
    LogBoundaryResponseError("UserService", "PhoneLogin", request->request_id(),
                             *response);
    return grpc::Status::OK;
}

grpc::Status UserGrpcService::GetUserInfo(grpc::ServerContext *,
                                          const zchat::GetUserInfoReq *request,
                                          zchat::GetUserInfoRsp *response) {
    ZCHAT_LOG_INFO("UserService::GetUserInfo request_id={}",
                   request->request_id());
    *response = service_->GetUserInfo(*request);
    LogBoundaryResponseError("UserService", "GetUserInfo",
                             request->request_id(), *response);
    return grpc::Status::OK;
}

grpc::Status
UserGrpcService::GetMultiUserInfo(grpc::ServerContext *,
                                  const zchat::GetMultiUserInfoReq *request,
                                  zchat::GetMultiUserInfoRsp *response) {
    ZCHAT_LOG_INFO("UserService::GetMultiUserInfo request_id={}",
                   request->request_id());
    *response = service_->GetMultiUserInfo(*request);
    LogBoundaryResponseError("UserService", "GetMultiUserInfo",
                             request->request_id(), *response);
    return grpc::Status::OK;
}

grpc::Status UserGrpcService::SearchUsers(grpc::ServerContext *,
                                          const zchat::SearchUsersReq *request,
                                          zchat::SearchUsersRsp *response) {
    ZCHAT_LOG_INFO("UserService::SearchUsers request_id={}",
                   request->request_id());
    *response = service_->SearchUsers(*request);
    LogBoundaryResponseError("UserService", "SearchUsers",
                             request->request_id(), *response);
    return grpc::Status::OK;
}

grpc::Status
UserGrpcService::SetUserAvatar(grpc::ServerContext *,
                               const zchat::SetUserAvatarReq *request,
                               zchat::SetUserAvatarRsp *response) {
    ZCHAT_LOG_INFO("UserService::SetUserAvatar request_id={}",
                   request->request_id());
    *response = service_->SetAvatar(*request);
    LogBoundaryResponseError("UserService", "SetUserAvatar",
                             request->request_id(), *response);
    return grpc::Status::OK;
}

grpc::Status
UserGrpcService::SetUserNickname(grpc::ServerContext *,
                                 const zchat::SetUserNicknameReq *request,
                                 zchat::SetUserNicknameRsp *response) {
    ZCHAT_LOG_INFO("UserService::SetUserNickname request_id={}",
                   request->request_id());
    *response = service_->SetNickname(*request);
    LogBoundaryResponseError("UserService", "SetUserNickname",
                             request->request_id(), *response);
    return grpc::Status::OK;
}

grpc::Status
UserGrpcService::SetUserDescription(grpc::ServerContext *,
                                    const zchat::SetUserDescriptionReq *request,
                                    zchat::SetUserDescriptionRsp *response) {
    ZCHAT_LOG_INFO("UserService::SetUserDescription request_id={}",
                   request->request_id());
    *response = service_->SetDescription(*request);
    LogBoundaryResponseError("UserService", "SetUserDescription",
                             request->request_id(), *response);
    return grpc::Status::OK;
}

grpc::Status
UserGrpcService::SetUserPhoneNumber(grpc::ServerContext *,
                                    const zchat::SetUserPhoneNumberReq *request,
                                    zchat::SetUserPhoneNumberRsp *response) {
    ZCHAT_LOG_INFO("UserService::SetUserPhoneNumber request_id={}",
                   request->request_id());
    *response = service_->SetPhone(*request);
    LogBoundaryResponseError("UserService", "SetUserPhoneNumber",
                             request->request_id(), *response);
    return grpc::Status::OK;
}

} // namespace zchat
