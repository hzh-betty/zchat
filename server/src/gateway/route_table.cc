#include "gateway/route_table.h"

#include <chrono>
#include <memory>
#include <string>
#include <utility>

#include <grpcpp/grpcpp.h>

#include "common/common_errors.h"
#include "common/logger.h"
#include "common/protobuf_http.h"
#include "common/result.h"
#include "common/session_store.h"
#include "file.grpc.pb.h"
#include "friend.grpc.pb.h"
#include "gateway/grpc_service_clients.h"
#include "message.grpc.pb.h"
#include "speech.grpc.pb.h"
#include "transmite.grpc.pb.h"
#include "user.grpc.pb.h"

namespace zchat {
namespace {

template <typename Response>
drogon::HttpResponsePtr ErrorResponse(const AppError &error) {
    Response response;
    response.set_success(false);
    response.set_errmsg(FormatErrorForClient(error));
    return ProtobufResponse(response);
}

template <typename Service, typename Request, typename Response,
          typename AsyncCall>
void CallStubAsync(
    GrpcServiceClients &clients, const std::string &service_name,
    AsyncCall async_call, const std::string &body,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    std::chrono::seconds deadline = std::chrono::seconds(5)) {
    auto request = std::make_shared<Request>();
    if (!request->ParseFromString(body)) {
        ZCHAT_LOG_WARN("grpc protobuf parse failed, body size={}B",
                       body.size());
        callback(
            ErrorResponse<Response>(common_errors::RequestBodyParseFailed()));
        return;
    }
    auto endpoint = clients.discovery().Endpoint(service_name);
    if (!endpoint.ok()) {
        callback(ErrorResponse<Response>(endpoint.error()));
        return;
    }
    auto stub = std::shared_ptr<typename Service::Stub>(
        Service::NewStub(clients.GetOrCreateChannel(endpoint.value())));
    auto response = std::make_shared<Response>();
    auto context = std::make_shared<grpc::ClientContext>();
    context->set_deadline(std::chrono::system_clock::now() + deadline);
    async_call(stub.get(), context.get(), request.get(), response.get(),
               [stub, request, response, context,
                callback = std::move(callback)](grpc::Status status) {
                   if (!status.ok()) {
                       callback(ErrorResponse<Response>(
                           AppError::WithCode(ErrorCode::kExternalServiceError,
                                              "grpc request failed")
                               .WithDetail(status.error_message())));
                       return;
                   }
                   callback(ProtobufResponse(*response));
               });
}

void CallTransmiteAsync(
    GrpcServiceClients &clients, const std::string &body,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    auto request = std::make_shared<zchat::NewMessageReq>();
    if (!request->ParseFromString(body)) {
        ZCHAT_LOG_WARN("grpc protobuf parse failed, body size={}B",
                       body.size());
        callback(ErrorResponse<zchat::NewMessageRsp>(
            common_errors::RequestBodyParseFailed()));
        return;
    }
    auto endpoint = clients.discovery().Endpoint("transmite_service");
    if (!endpoint.ok()) {
        callback(ErrorResponse<zchat::NewMessageRsp>(endpoint.error()));
        return;
    }
    auto stub = std::shared_ptr<zchat::MsgTransmitService::Stub>(
        zchat::MsgTransmitService::NewStub(
            clients.GetOrCreateChannel(endpoint.value())));
    auto target_response = std::make_shared<zchat::GetTransmitTargetRsp>();
    auto context = std::make_shared<grpc::ClientContext>();
    context->set_deadline(std::chrono::system_clock::now() +
                          std::chrono::seconds(5));
    stub->async()->GetTransmitTarget(
        context.get(), request.get(), target_response.get(),
        [stub, request, target_response, context,
         callback = std::move(callback)](grpc::Status status) {
            if (!status.ok()) {
                ZCHAT_LOG_ERROR("GetTransmitTarget rpc failed: error_code={}, "
                                "error_message={}",
                                static_cast<int>(status.error_code()),
                                status.error_message());
                callback(ErrorResponse<zchat::NewMessageRsp>(
                    AppError::WithCode(ErrorCode::kExternalServiceError,
                                       "grpc request failed")
                        .WithDetail(status.error_message())));
                return;
            }
            zchat::NewMessageRsp response;
            response.set_request_id(target_response->request_id());
            response.set_success(target_response->success());
            response.set_errmsg(target_response->errmsg());
            callback(ProtobufResponse(response));
        });
}

template <typename Request, typename Response, typename Service,
          typename StubCall>
void AuthAndForward(
    SessionStore &sessions, GrpcServiceClients &clients,
    const std::string &service_name, StubCall stub_call,
    const std::string &body,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    std::chrono::seconds deadline = std::chrono::seconds(5)) {
    auto request = std::make_shared<Request>();
    if (!request->ParseFromString(body)) {
        ZCHAT_LOG_WARN("gateway auth protobuf parse failed, body size={}B",
                       body.size());
        callback(
            ErrorResponse<Response>(common_errors::RequestBodyParseFailed()));
        return;
    }
    sessions.GetUserIdAsync(
        request->session_id(),
        [request, &clients, service_name, stub_call, deadline,
         callback = std::move(callback)](
            Result<std::optional<std::string>> user_id) mutable {
            if (!user_id.ok()) {
                ZCHAT_LOG_WARN("gateway auth redis failed: {}",
                               user_id.error().message);
                callback(ErrorResponse<Response>(user_id.error()));
                return;
            }
            if (!user_id.value().has_value()) {
                ZCHAT_LOG_WARN("gateway auth rejected: invalid session");
                callback(
                    ErrorResponse<Response>(common_errors::SessionExpired()));
                return;
            }
            request->set_user_id(user_id.value().value());
            CallStubAsync<Service, Request, Response>(
                clients, service_name, stub_call, request->SerializeAsString(),
                std::move(callback), deadline);
        });
}

void AuthAndForwardTransmite(
    SessionStore &sessions, GrpcServiceClients &clients,
    const std::string &body,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    auto request = std::make_shared<zchat::NewMessageReq>();
    if (!request->ParseFromString(body)) {
        ZCHAT_LOG_WARN("gateway auth protobuf parse failed, body size={}B",
                       body.size());
        callback(ErrorResponse<zchat::NewMessageRsp>(
            common_errors::RequestBodyParseFailed()));
        return;
    }
    sessions.GetUserIdAsync(
        request->session_id(),
        [request, &clients, callback = std::move(callback)](
            Result<std::optional<std::string>> user_id) mutable {
            if (!user_id.ok()) {
                ZCHAT_LOG_WARN("gateway auth redis failed: {}",
                               user_id.error().message);
                callback(ErrorResponse<zchat::NewMessageRsp>(user_id.error()));
                return;
            }
            if (!user_id.value().has_value()) {
                ZCHAT_LOG_WARN("gateway auth rejected: invalid session");
                callback(ErrorResponse<zchat::NewMessageRsp>(
                    common_errors::SessionExpired()));
                return;
            }
            request->set_user_id(user_id.value().value());
            CallTransmiteAsync(clients, request->SerializeAsString(),
                               std::move(callback));
        });
}

using HandlerFn = std::function<void(
    SessionStore *, GrpcServiceClients &, const std::string &,
    std::function<void(const drogon::HttpResponsePtr &)> &&)>;

template <typename Service, typename Req, typename Rsp, typename Method>
HandlerFn PublicHandler(const std::string &svc, Method method,
                        std::chrono::seconds deadline) {
    return [svc, method, deadline](
               SessionStore *, GrpcServiceClients &clients,
               const std::string &body,
               std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CallStubAsync<Service, Req, Rsp>(clients, svc, method, body,
                                         std::move(cb), deadline);
    };
}

template <typename Service, typename Req, typename Rsp, typename Method>
HandlerFn AuthHandler(const std::string &svc, Method method,
                      std::chrono::seconds deadline) {
    return [svc, method, deadline](
               SessionStore *sessions, GrpcServiceClients &clients,
               const std::string &body,
               std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        AuthAndForward<Req, Rsp, Service>(*sessions, clients, svc, method, body,
                                          std::move(cb), deadline);
    };
}

const std::vector<RouteEntry> &BuildRouteTable() {
    static const auto kDefault = std::chrono::seconds(5);
    static const auto kFile = std::chrono::seconds(30);
    static const std::vector<RouteEntry> routes =
        {
            {"/service/user/get_phone_verify_code", "user_service", false,
             kDefault,
             PublicHandler<zchat::UserService, zchat::PhoneVerifyCodeReq,
                           zchat::PhoneVerifyCodeRsp>(
                 "user_service",
                 [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                     s->async()->GetPhoneVerifyCode(c, q, r, std::move(cb));
                 },
                 kDefault)},
            {"/service/user/username_register", "user_service", false, kDefault,
             PublicHandler<zchat::UserService, zchat::UserRegisterReq,
                           zchat::UserRegisterRsp>(
                 "user_service",
                 [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                     s->async()->UserRegister(c, q, r, std::move(cb));
                 },
                 kDefault)},
            {"/service/user/username_login", "user_service", false, kDefault,
             PublicHandler<zchat::UserService, zchat::UserLoginReq,
                           zchat::UserLoginRsp>(
                 "user_service",
                 [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                     s->async()->UserLogin(c, q, r, std::move(cb));
                 },
                 kDefault)},
            {"/service/user/phone_register", "user_service", false, kDefault,
             PublicHandler<zchat::UserService, zchat::PhoneRegisterReq,
                           zchat::PhoneRegisterRsp>(
                 "user_service",
                 [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                     s->async()->PhoneRegister(c, q, r, std::move(cb));
                 },
                 kDefault)},
            {"/service/user/phone_login", "user_service", false, kDefault,
             PublicHandler<zchat::UserService, zchat::PhoneLoginReq,
                           zchat::PhoneLoginRsp>(
                 "user_service",
                 [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                     s->async()->PhoneLogin(c, q, r, std::move(cb));
                 },
                 kDefault)},
            {"/service/user/get_user_info", "user_service", true, kDefault,
             AuthHandler<zchat::UserService, zchat::GetUserInfoReq,
                         zchat::GetUserInfoRsp>(
                 "user_service",
                 [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                     s->async()->GetUserInfo(c, q, r, std::move(cb));
                 },
                 kDefault)},
            {"/service/user/set_avatar", "user_service", true, kDefault,
             AuthHandler<zchat::UserService, zchat::SetUserAvatarReq,
                         zchat::SetUserAvatarRsp>(
                 "user_service",
                 [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                     s->async()->SetUserAvatar(c, q, r, std::move(cb));
                 },
                 kDefault)},
            {"/service/user/set_nickname", "user_service", true, kDefault,
             AuthHandler<zchat::UserService, zchat::SetUserNicknameReq,
                         zchat::SetUserNicknameRsp>(
                 "user_service",
                 [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                     s->async()->SetUserNickname(c, q, r, std::move(cb));
                 },
                 kDefault)},
            {"/service/user/set_description", "user_service", true, kDefault,
             AuthHandler<zchat::UserService, zchat::SetUserDescriptionReq,
                         zchat::SetUserDescriptionRsp>(
                 "user_service",
                 [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                     s->async()->SetUserDescription(c, q, r, std::move(cb));
                 },
                 kDefault)},
            {"/service/user/set_phone", "user_service", true, kDefault,
             AuthHandler<zchat::UserService, zchat::SetUserPhoneNumberReq,
                         zchat::SetUserPhoneNumberRsp>(
                 "user_service",
                 [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                     s->async()->SetUserPhoneNumber(c, q, r, std::move(cb));
                 },
                 kDefault)},
            {"/service/friend/get_friend_list", "friend_service", true,
             kDefault,
             AuthHandler<zchat::FriendService, zchat::GetFriendListReq,
                         zchat::GetFriendListRsp>(
                 "friend_service",
                 [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                     s->async()->GetFriendList(c, q, r, std::move(cb));
                 },
                 kDefault)},
            {"/service/friend/get_chat_session_list", "friend_service", true,
             kDefault,
             AuthHandler<zchat::FriendService, zchat::GetChatSessionListReq,
                         zchat::GetChatSessionListRsp>(
                 "friend_service",
                 [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                     s->async()->GetChatSessionList(c, q, r, std::move(cb));
                 },
                 kDefault)},
            {"/service/friend/get_pending_friend_events", "friend_service",
             true, kDefault,
             AuthHandler<zchat::FriendService,
                         zchat::GetPendingFriendEventListReq,
                         zchat::GetPendingFriendEventListRsp>(
                 "friend_service",
                 [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                     s->async()->GetPendingFriendEventList(c, q, r,
                                                           std::move(cb));
                 },
                 kDefault)},
            {"/service/friend/remove_friend", "friend_service", true, kDefault,
             AuthHandler<zchat::FriendService, zchat::FriendRemoveReq,
                         zchat::FriendRemoveRsp>(
                 "friend_service",
                 [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                     s->async()->FriendRemove(c, q, r, std::move(cb));
                 },
                 kDefault)},
            {"/service/friend/add_friend_apply", "friend_service", true,
             kDefault,
             AuthHandler<zchat::FriendService, zchat::FriendAddReq,
                         zchat::FriendAddRsp>(
                 "friend_service",
                 [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                     s->async()->FriendAdd(c, q, r, std::move(cb));
                 },
                 kDefault)},
            {"/service/friend/add_friend_process", "friend_service", true,
             kDefault,
             AuthHandler<zchat::FriendService, zchat::FriendAddProcessReq,
                         zchat::FriendAddProcessRsp>(
                 "friend_service",
                 [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                     s->async()->FriendAddProcess(c, q, r, std::move(cb));
                 },
                 kDefault)},
            {"/service/friend/create_chat_session", "friend_service", true,
             kDefault,
             AuthHandler<zchat::FriendService, zchat::ChatSessionCreateReq,
                         zchat::ChatSessionCreateRsp>(
                 "friend_service",
                 [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                     s->async()->ChatSessionCreate(c, q, r, std::move(cb));
                 },
                 kDefault)},
            {"/service/friend/get_chat_session_member", "friend_service", true,
             kDefault,
             AuthHandler<zchat::FriendService, zchat::GetChatSessionMemberReq,
                         zchat::GetChatSessionMemberRsp>(
                 "friend_service",
                 [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                     s->async()->GetChatSessionMember(c, q, r, std::move(cb));
                 },
                 kDefault)},
            {"/service/friend/search_friend", "friend_service", true, kDefault,
             AuthHandler<zchat::FriendService, zchat::FriendSearchReq,
                         zchat::FriendSearchRsp>(
                 "friend_service",
                 [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                     s->async()->FriendSearch(c, q, r, std::move(cb));
                 },
                 kDefault)},
            {"/service/message_storage/get_recent", "message_service", true,
             kDefault,
             AuthHandler<zchat::MsgStorageService, zchat::GetRecentMsgReq,
                         zchat::GetRecentMsgRsp>(
                 "message_service",
                 [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                     s->async()->GetRecentMsg(c, q, r, std::move(cb));
                 },
                 kDefault)},
            {"/service/message_storage/get_history", "message_service", true,
             kDefault,
             AuthHandler<zchat::MsgStorageService, zchat::GetHistoryMsgReq,
                         zchat::GetHistoryMsgRsp>(
                 "message_service",
                 [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                     s->async()->GetHistoryMsg(c, q, r, std::move(cb));
                 },
                 kDefault)},
            {"/service/message_storage/search_history", "message_service", true,
             kDefault,
             AuthHandler<zchat::MsgStorageService, zchat::MsgSearchReq,
                         zchat::MsgSearchRsp>(
                 "message_service",
                 [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                     s->async()->MsgSearch(c, q, r, std::move(cb));
                 },
                 kDefault)},
            {"/service/message_transmit/new_message", "transmite_service", true,
             kDefault,
             [](
                 SessionStore *sessions, GrpcServiceClients &clients,
                 const std::string &body,
                 std::function<void(const drogon::HttpResponsePtr &)>
                     &&callback) {
                 AuthAndForwardTransmite(*sessions, clients, body,
                                         std::move(callback));
             }},
            {"/service/file/get_single_file", "file_service", true, kFile,
             AuthHandler<zchat::FileService, zchat::GetSingleFileReq,
                         zchat::GetSingleFileRsp>(
                 "file_service",
                 [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                     s->async()->GetSingleFile(c, q, r, std::move(cb));
                 },
                 kFile)},
            {"/service/file/get_multi_file", "file_service", true, kFile,
             AuthHandler<zchat::FileService, zchat::GetMultiFileReq,
                         zchat::GetMultiFileRsp>(
                 "file_service",
                 [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                     s->async()->GetMultiFile(c, q, r, std::move(cb));
                 },
                 kFile)},
            {"/service/file/put_single_file", "file_service", true, kFile,
             AuthHandler<zchat::FileService, zchat::PutSingleFileReq,
                         zchat::PutSingleFileRsp>(
                 "file_service",
                 [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                     s->async()->PutSingleFile(c, q, r, std::move(cb));
                 },
                 kFile)},
            {"/service/file/put_multi_file", "file_service", true, kFile,
             AuthHandler<zchat::FileService, zchat::PutMultiFileReq,
                         zchat::PutMultiFileRsp>(
                 "file_service",
                 [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                     s->async()->PutMultiFile(c, q, r, std::move(cb));
                 },
                 kFile)},
            {"/service/speech/recognition", "speech_service", true, kDefault,
             AuthHandler<zchat::SpeechService, zchat::SpeechRecognitionReq,
                         zchat::SpeechRecognitionRsp>(
                 "speech_service",
                 [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                     s->async()->SpeechRecognition(c, q, r, std::move(cb));
                 },
                 kDefault)},
        };
    return routes;
}

} // namespace

const RouteEntry *FindRoute(const std::string &path) {
    for (const auto &entry : GetAllRoutes()) {
        if (entry.path == path) {
            return &entry;
        }
    }
    return nullptr;
}

const std::vector<RouteEntry> &GetAllRoutes() { return BuildRouteTable(); }

} // namespace zchat
