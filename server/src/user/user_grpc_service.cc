#include "user/user_grpc_service.h"

#include <utility>

#include "common/error_response.h"
#include "common/grpc_callback.h"
#include "common/logger.h"

namespace zchat {

UserGrpcService::UserGrpcService(
    std::shared_ptr<UserApplicationService> service)
    : service_(std::move(service)) {}

#define ZCHAT_USER_RPC(method_name, req_type, rsp_type, coro_name)             \
    grpc::ServerUnaryReactor *UserGrpcService::method_name(                    \
        grpc::CallbackServerContext *, const zchat::req_type *request,         \
        zchat::rsp_type *response) {                                           \
        ZCHAT_LOG_INFO("UserService::" #method_name " request_id={}",          \
                       request->request_id());                                 \
        return new CoroUnaryReactor<zchat::rsp_type>(                          \
            [this, req = *request]() -> drogon::Task<zchat::rsp_type> {        \
                co_return co_await service_->coro_name(req);                   \
            },                                                                 \
            response, "UserService", #method_name, request->request_id());     \
    }

ZCHAT_USER_RPC(UserRegister, UserRegisterReq, UserRegisterRsp,
               RegisterByNicknameCoro)
ZCHAT_USER_RPC(UserLogin, UserLoginReq, UserLoginRsp, LoginByNicknameCoro)
ZCHAT_USER_RPC(GetPhoneVerifyCode, PhoneVerifyCodeReq, PhoneVerifyCodeRsp,
               GetPhoneVerifyCodeCoro)
ZCHAT_USER_RPC(PhoneRegister, PhoneRegisterReq, PhoneRegisterRsp,
               RegisterByPhoneCoro)
ZCHAT_USER_RPC(PhoneLogin, PhoneLoginReq, PhoneLoginRsp, LoginByPhoneCoro)
ZCHAT_USER_RPC(GetUserInfo, GetUserInfoReq, GetUserInfoRsp, GetUserInfoCoro)
ZCHAT_USER_RPC(GetMultiUserInfo, GetMultiUserInfoReq, GetMultiUserInfoRsp,
               GetMultiUserInfoCoro)
ZCHAT_USER_RPC(SearchUsers, SearchUsersReq, SearchUsersRsp, SearchUsersCoro)
ZCHAT_USER_RPC(SetUserAvatar, SetUserAvatarReq, SetUserAvatarRsp, SetAvatarCoro)
ZCHAT_USER_RPC(SetUserNickname, SetUserNicknameReq, SetUserNicknameRsp,
               SetNicknameCoro)
ZCHAT_USER_RPC(SetUserDescription, SetUserDescriptionReq, SetUserDescriptionRsp,
               SetDescriptionCoro)
ZCHAT_USER_RPC(SetUserPhoneNumber, SetUserPhoneNumberReq, SetUserPhoneNumberRsp,
               SetPhoneCoro)

} // namespace zchat
