#include "gateway/route_table.h"

#include <chrono>
#include <memory>
#include <string>
#include <utility>

#include <grpcpp/grpcpp.h>

#include "common/common_errors.h"
#include "common/grpc_awaiter.h"
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

template <typename Service, typename Req, typename Rsp, typename AsyncCall>
drogon::Task<drogon::HttpResponsePtr>
CallStubCoro(GrpcServiceClients &clients, const char *service_name,
             AsyncCall async_call, const std::string &body,
             std::chrono::seconds deadline) {
    auto request = Req();
    if (!request.ParseFromString(body)) {
        co_return ErrorResponse<Rsp>(common_errors::RequestBodyParseFailed());
    }
    auto rsp = co_await CallUnaryCoro<Service, Req, Rsp>(
        clients.discovery(), clients.channel_pool(), service_name, async_call,
        request, deadline);
    if (!rsp.ok()) {
        co_return ErrorResponse<Rsp>(rsp.error());
    }
    co_return ProtobufResponse(rsp.value());
}

drogon::Task<drogon::HttpResponsePtr>
CallTransmiteCoro(GrpcServiceClients &clients, const std::string &body) {
    auto request = zchat::NewMessageReq();
    if (!request.ParseFromString(body)) {
        co_return ErrorResponse<zchat::NewMessageRsp>(
            common_errors::RequestBodyParseFailed());
    }
    auto rsp =
        co_await CallUnaryCoro<zchat::MsgTransmitService, zchat::NewMessageReq,
                               zchat::GetTransmitTargetRsp>(
            clients.discovery(), clients.channel_pool(), "transmite_service",
            [](zchat::MsgTransmitService::Stub *stub, grpc::ClientContext *ctx,
               const zchat::NewMessageReq *req,
               zchat::GetTransmitTargetRsp *rsp,
               std::function<void(grpc::Status)> cb) {
                stub->async()->GetTransmitTarget(ctx, req, rsp, std::move(cb));
            },
            request, std::chrono::seconds(5));
    if (!rsp.ok()) {
        co_return ErrorResponse<zchat::NewMessageRsp>(rsp.error());
    }
    zchat::NewMessageRsp response;
    response.set_request_id(rsp.value().request_id());
    response.set_success(rsp.value().success());
    response.set_errmsg(rsp.value().errmsg());
    co_return ProtobufResponse(response);
}

template <typename Req, typename Rsp, typename Service, typename AsyncCall>
drogon::Task<drogon::HttpResponsePtr>
AuthAndForwardCoro(SessionStore &sessions, GrpcServiceClients &clients,
                   const char *service_name, AsyncCall async_call,
                   const std::string &body, std::chrono::seconds deadline) {
    auto request = Req();
    if (!request.ParseFromString(body)) {
        co_return ErrorResponse<Rsp>(common_errors::RequestBodyParseFailed());
    }
    auto user_id = co_await sessions.GetUserIdCoro(request.session_id());
    if (!user_id.ok()) {
        co_return ErrorResponse<Rsp>(user_id.error());
    }
    if (!user_id.value().has_value()) {
        co_return ErrorResponse<Rsp>(common_errors::SessionExpired());
    }
    request.set_user_id(user_id.value().value());
    auto rsp = co_await CallUnaryCoro<Service, Req, Rsp>(
        clients.discovery(), clients.channel_pool(), service_name, async_call,
        request, deadline);
    if (!rsp.ok()) {
        co_return ErrorResponse<Rsp>(rsp.error());
    }
    co_return ProtobufResponse(rsp.value());
}

drogon::Task<drogon::HttpResponsePtr>
AuthAndForwardTransmiteCoro(SessionStore &sessions, GrpcServiceClients &clients,
                            const std::string &body) {
    auto request = zchat::NewMessageReq();
    if (!request.ParseFromString(body)) {
        co_return ErrorResponse<zchat::NewMessageRsp>(
            common_errors::RequestBodyParseFailed());
    }
    auto user_id = co_await sessions.GetUserIdCoro(request.session_id());
    if (!user_id.ok()) {
        co_return ErrorResponse<zchat::NewMessageRsp>(user_id.error());
    }
    if (!user_id.value().has_value()) {
        co_return ErrorResponse<zchat::NewMessageRsp>(
            common_errors::SessionExpired());
    }
    request.set_user_id(user_id.value().value());
    co_return co_await CallTransmiteCoro(clients, request.SerializeAsString());
}

using HandlerFn = std::function<drogon::Task<drogon::HttpResponsePtr>(
    SessionStore *, GrpcServiceClients &, const std::string &)>;

template <typename Service, typename Req, typename Rsp, typename AsyncCall>
HandlerFn PublicHandler(const char *svc, AsyncCall async_call,
                        std::chrono::seconds deadline) {
    return
        [svc, async_call, deadline](
            SessionStore *, GrpcServiceClients &clients,
            const std::string &body) -> drogon::Task<drogon::HttpResponsePtr> {
            co_return co_await CallStubCoro<Service, Req, Rsp>(
                clients, svc, async_call, body, deadline);
        };
}

template <typename Service, typename Req, typename Rsp, typename AsyncCall>
HandlerFn AuthHandler(const char *svc, AsyncCall async_call,
                      std::chrono::seconds deadline) {
    return
        [svc, async_call, deadline](
            SessionStore *sessions, GrpcServiceClients &clients,
            const std::string &body) -> drogon::Task<drogon::HttpResponsePtr> {
            co_return co_await AuthAndForwardCoro<Req, Rsp, Service>(
                *sessions, clients, svc, async_call, body, deadline);
        };
}

template <typename Service, typename Req, typename Rsp, typename AsyncCall>
RouteEntry MakePublic(const char *path, const char *svc, AsyncCall ac,
                      std::chrono::seconds deadline) {
    return {path, svc, false, deadline,
            PublicHandler<Service, Req, Rsp>(svc, ac, deadline)};
}

template <typename Service, typename Req, typename Rsp, typename AsyncCall>
RouteEntry MakeAuth(const char *path, const char *svc, AsyncCall ac,
                    std::chrono::seconds deadline) {
    return {path, svc, true, deadline,
            AuthHandler<Service, Req, Rsp>(svc, ac, deadline)};
}

#define AC(S, method)                                                          \
    [](zchat::S::Stub *s, grpc::ClientContext *c, const auto *q, auto *r,      \
       std::function<void(grpc::Status)> cb) {                                 \
        s->async()->method(c, q, r, std::move(cb));                            \
    }

} // namespace

const std::vector<RouteEntry> &BuildRouteTable() {
    static const auto kD = std::chrono::seconds(5);
    static const auto kF = std::chrono::seconds(30);
    static const std::vector<RouteEntry> routes = {
        MakePublic<zchat::UserService, zchat::PhoneVerifyCodeReq,
                   zchat::PhoneVerifyCodeRsp>(
            "/service/user/get_phone_verify_code", "user_service",
            AC(UserService, GetPhoneVerifyCode), kD),
        MakePublic<zchat::UserService, zchat::UserRegisterReq,
                   zchat::UserRegisterRsp>("/service/user/username_register",
                                           "user_service",
                                           AC(UserService, UserRegister), kD),
        MakePublic<zchat::UserService, zchat::UserLoginReq,
                   zchat::UserLoginRsp>("/service/user/username_login",
                                        "user_service",
                                        AC(UserService, UserLogin), kD),
        MakePublic<zchat::UserService, zchat::PhoneRegisterReq,
                   zchat::PhoneRegisterRsp>("/service/user/phone_register",
                                            "user_service",
                                            AC(UserService, PhoneRegister), kD),
        MakePublic<zchat::UserService, zchat::PhoneLoginReq,
                   zchat::PhoneLoginRsp>("/service/user/phone_login",
                                         "user_service",
                                         AC(UserService, PhoneLogin), kD),
        MakeAuth<zchat::UserService, zchat::GetUserInfoReq,
                 zchat::GetUserInfoRsp>("/service/user/get_user_info",
                                        "user_service",
                                        AC(UserService, GetUserInfo), kD),
        MakeAuth<zchat::UserService, zchat::SetUserAvatarReq,
                 zchat::SetUserAvatarRsp>("/service/user/set_avatar",
                                          "user_service",
                                          AC(UserService, SetUserAvatar), kD),
        MakeAuth<zchat::UserService, zchat::SetUserNicknameReq,
                 zchat::SetUserNicknameRsp>(
            "/service/user/set_nickname", "user_service",
            AC(UserService, SetUserNickname), kD),
        MakeAuth<zchat::UserService, zchat::SetUserDescriptionReq,
                 zchat::SetUserDescriptionRsp>(
            "/service/user/set_description", "user_service",
            AC(UserService, SetUserDescription), kD),
        MakeAuth<zchat::UserService, zchat::SetUserPhoneNumberReq,
                 zchat::SetUserPhoneNumberRsp>(
            "/service/user/set_phone", "user_service",
            AC(UserService, SetUserPhoneNumber), kD),
        MakeAuth<zchat::FriendService, zchat::GetFriendListReq,
                 zchat::GetFriendListRsp>("/service/friend/get_friend_list",
                                          "friend_service",
                                          AC(FriendService, GetFriendList), kD),
        MakeAuth<zchat::FriendService, zchat::GetChatSessionListReq,
                 zchat::GetChatSessionListRsp>(
            "/service/friend/get_chat_session_list", "friend_service",
            AC(FriendService, GetChatSessionList), kD),
        MakeAuth<zchat::FriendService, zchat::GetPendingFriendEventListReq,
                 zchat::GetPendingFriendEventListRsp>(
            "/service/friend/get_pending_friend_events", "friend_service",
            AC(FriendService, GetPendingFriendEventList), kD),
        MakeAuth<zchat::FriendService, zchat::FriendRemoveReq,
                 zchat::FriendRemoveRsp>("/service/friend/remove_friend",
                                         "friend_service",
                                         AC(FriendService, FriendRemove), kD),
        MakeAuth<zchat::FriendService, zchat::FriendAddReq,
                 zchat::FriendAddRsp>("/service/friend/add_friend_apply",
                                      "friend_service",
                                      AC(FriendService, FriendAdd), kD),
        MakeAuth<zchat::FriendService, zchat::FriendAddProcessReq,
                 zchat::FriendAddProcessRsp>(
            "/service/friend/add_friend_process", "friend_service",
            AC(FriendService, FriendAddProcess), kD),
        MakeAuth<zchat::FriendService, zchat::ChatSessionCreateReq,
                 zchat::ChatSessionCreateRsp>(
            "/service/friend/create_chat_session", "friend_service",
            AC(FriendService, ChatSessionCreate), kD),
        MakeAuth<zchat::FriendService, zchat::GetChatSessionMemberReq,
                 zchat::GetChatSessionMemberRsp>(
            "/service/friend/get_chat_session_member", "friend_service",
            AC(FriendService, GetChatSessionMember), kD),
        MakeAuth<zchat::FriendService, zchat::FriendSearchReq,
                 zchat::FriendSearchRsp>("/service/friend/search_friend",
                                         "friend_service",
                                         AC(FriendService, FriendSearch), kD),
        MakeAuth<zchat::MsgStorageService, zchat::GetRecentMsgReq,
                 zchat::GetRecentMsgRsp>(
            "/service/message_storage/get_recent", "message_service",
            AC(MsgStorageService, GetRecentMsg), kD),
        MakeAuth<zchat::MsgStorageService, zchat::GetHistoryMsgReq,
                 zchat::GetHistoryMsgRsp>(
            "/service/message_storage/get_history", "message_service",
            AC(MsgStorageService, GetHistoryMsg), kD),
        MakeAuth<zchat::MsgStorageService, zchat::MsgSearchReq,
                 zchat::MsgSearchRsp>("/service/message_storage/search_history",
                                      "message_service",
                                      AC(MsgStorageService, MsgSearch), kD),
        {"/service/message_transmit/new_message", "transmite_service", true, kD,
         [](SessionStore *sessions, GrpcServiceClients &clients,
            const std::string &body) -> drogon::Task<drogon::HttpResponsePtr> {
             co_return co_await AuthAndForwardTransmiteCoro(*sessions, clients,
                                                            body);
         }},
        MakeAuth<zchat::FileService, zchat::GetSingleFileReq,
                 zchat::GetSingleFileRsp>("/service/file/get_single_file",
                                          "file_service",
                                          AC(FileService, GetSingleFile), kF),
        MakeAuth<zchat::FileService, zchat::GetMultiFileReq,
                 zchat::GetMultiFileRsp>("/service/file/get_multi_file",
                                         "file_service",
                                         AC(FileService, GetMultiFile), kF),
        MakeAuth<zchat::FileService, zchat::PutSingleFileReq,
                 zchat::PutSingleFileRsp>("/service/file/put_single_file",
                                          "file_service",
                                          AC(FileService, PutSingleFile), kF),
        MakeAuth<zchat::FileService, zchat::PutMultiFileReq,
                 zchat::PutMultiFileRsp>("/service/file/put_multi_file",
                                         "file_service",
                                         AC(FileService, PutMultiFile), kF),
        MakeAuth<zchat::SpeechService, zchat::SpeechRecognitionReq,
                 zchat::SpeechRecognitionRsp>(
            "/service/speech/recognition", "speech_service",
            AC(SpeechService, SpeechRecognition), kD),
    };
#undef AC
    return routes;
}

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
