#include "gateway/gateway_controller.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "common/common_errors.h"
#include "common/logger.h"
#include "common/protobuf_http.h"
#include "common/result.h"
#include "file.pb.h"
#include "friend.pb.h"
#include "message.pb.h"
#include "speech.pb.h"
#include "transmite.pb.h"
#include "user.pb.h"

namespace zchat {
namespace {

template <typename Response>
drogon::HttpResponsePtr GatewayErrorResponse(const AppError &error) {
    Response response;
    response.set_success(false);
    response.set_errmsg(FormatErrorForClient(error));
    return ProtobufResponse(response);
}

template <typename Request, typename Response>
void AuthenticateAndInjectUserAsync(
    SessionStore &sessions, const std::string &body,
    GrpcServiceClients &grpc_clients, const std::string &path,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    auto request = std::make_shared<Request>();
    if (!request->ParseFromString(body)) {
        ZCHAT_LOG_WARN("gateway auth protobuf parse failed, body size={}B",
                       body.size());
        callback(GatewayErrorResponse<Response>(
            common_errors::RequestBodyParseFailed()));
        return;
    }
    sessions.GetUserIdAsync(
        request->session_id(),
        [request, &grpc_clients, path, callback = std::move(callback)](
            Result<std::optional<std::string>> user_id) mutable {
            if (!user_id.ok()) {
                ZCHAT_LOG_WARN("gateway auth redis failed: {}",
                               user_id.error().message);
                callback(GatewayErrorResponse<Response>(user_id.error()));
                return;
            }
            if (!user_id.value().has_value()) {
                ZCHAT_LOG_WARN("gateway auth rejected: invalid session");
                callback(GatewayErrorResponse<Response>(
                    common_errors::SessionExpired()));
                return;
            }
            request->set_user_id(user_id.value().value());
            grpc_clients.Forward(path, request->SerializeAsString(),
                                 std::move(callback));
        });
}

bool IsPublicPath(const std::string &path) {
    return path == "/service/user/get_phone_verify_code" ||
           path == "/service/user/username_register" ||
           path == "/service/user/username_login" ||
           path == "/service/user/phone_register" ||
           path == "/service/user/phone_login";
}

void PrepareForwardBodyAsync(
    SessionStore &sessions, const std::string &path, const std::string &body,
    GrpcServiceClients &grpc_clients,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    if (IsPublicPath(path)) {
        grpc_clients.Forward(path, body, std::move(callback));
        return;
    }
    if (path == "/service/user/get_user_info") {
        return AuthenticateAndInjectUserAsync<zchat::GetUserInfoReq,
                                              zchat::GetUserInfoRsp>(
            sessions, body, grpc_clients, path, std::move(callback));
    }
    if (path == "/service/user/set_avatar") {
        return AuthenticateAndInjectUserAsync<zchat::SetUserAvatarReq,
                                              zchat::SetUserAvatarRsp>(
            sessions, body, grpc_clients, path, std::move(callback));
    }
    if (path == "/service/user/set_nickname") {
        return AuthenticateAndInjectUserAsync<zchat::SetUserNicknameReq,
                                              zchat::SetUserNicknameRsp>(
            sessions, body, grpc_clients, path, std::move(callback));
    }
    if (path == "/service/user/set_description") {
        return AuthenticateAndInjectUserAsync<zchat::SetUserDescriptionReq,
                                              zchat::SetUserDescriptionRsp>(
            sessions, body, grpc_clients, path, std::move(callback));
    }
    if (path == "/service/user/set_phone") {
        return AuthenticateAndInjectUserAsync<zchat::SetUserPhoneNumberReq,
                                              zchat::SetUserPhoneNumberRsp>(
            sessions, body, grpc_clients, path, std::move(callback));
    }
    if (path == "/service/friend/get_friend_list") {
        return AuthenticateAndInjectUserAsync<zchat::GetFriendListReq,
                                              zchat::GetFriendListRsp>(
            sessions, body, grpc_clients, path, std::move(callback));
    }
    if (path == "/service/friend/get_chat_session_list") {
        return AuthenticateAndInjectUserAsync<zchat::GetChatSessionListReq,
                                              zchat::GetChatSessionListRsp>(
            sessions, body, grpc_clients, path, std::move(callback));
    }
    if (path == "/service/friend/get_pending_friend_events") {
        return AuthenticateAndInjectUserAsync<
            zchat::GetPendingFriendEventListReq,
            zchat::GetPendingFriendEventListRsp>(sessions, body, grpc_clients,
                                                 path, std::move(callback));
    }
    if (path == "/service/friend/remove_friend") {
        return AuthenticateAndInjectUserAsync<zchat::FriendRemoveReq,
                                              zchat::FriendRemoveRsp>(
            sessions, body, grpc_clients, path, std::move(callback));
    }
    if (path == "/service/friend/add_friend_apply") {
        return AuthenticateAndInjectUserAsync<zchat::FriendAddReq,
                                              zchat::FriendAddRsp>(
            sessions, body, grpc_clients, path, std::move(callback));
    }
    if (path == "/service/friend/add_friend_process") {
        return AuthenticateAndInjectUserAsync<zchat::FriendAddProcessReq,
                                              zchat::FriendAddProcessRsp>(
            sessions, body, grpc_clients, path, std::move(callback));
    }
    if (path == "/service/friend/create_chat_session") {
        return AuthenticateAndInjectUserAsync<zchat::ChatSessionCreateReq,
                                              zchat::ChatSessionCreateRsp>(
            sessions, body, grpc_clients, path, std::move(callback));
    }
    if (path == "/service/friend/get_chat_session_member") {
        return AuthenticateAndInjectUserAsync<zchat::GetChatSessionMemberReq,
                                              zchat::GetChatSessionMemberRsp>(
            sessions, body, grpc_clients, path, std::move(callback));
    }
    if (path == "/service/friend/search_friend") {
        return AuthenticateAndInjectUserAsync<zchat::FriendSearchReq,
                                              zchat::FriendSearchRsp>(
            sessions, body, grpc_clients, path, std::move(callback));
    }
    if (path == "/service/message_storage/get_recent") {
        return AuthenticateAndInjectUserAsync<zchat::GetRecentMsgReq,
                                              zchat::GetRecentMsgRsp>(
            sessions, body, grpc_clients, path, std::move(callback));
    }
    if (path == "/service/message_storage/get_history") {
        return AuthenticateAndInjectUserAsync<zchat::GetHistoryMsgReq,
                                              zchat::GetHistoryMsgRsp>(
            sessions, body, grpc_clients, path, std::move(callback));
    }
    if (path == "/service/message_storage/search_history") {
        return AuthenticateAndInjectUserAsync<zchat::MsgSearchReq,
                                              zchat::MsgSearchRsp>(
            sessions, body, grpc_clients, path, std::move(callback));
    }
    if (path == "/service/message_transmit/new_message") {
        return AuthenticateAndInjectUserAsync<zchat::NewMessageReq,
                                              zchat::NewMessageRsp>(
            sessions, body, grpc_clients, path, std::move(callback));
    }
    if (path == "/service/file/get_single_file") {
        return AuthenticateAndInjectUserAsync<zchat::GetSingleFileReq,
                                              zchat::GetSingleFileRsp>(
            sessions, body, grpc_clients, path, std::move(callback));
    }
    if (path == "/service/file/get_multi_file") {
        return AuthenticateAndInjectUserAsync<zchat::GetMultiFileReq,
                                              zchat::GetMultiFileRsp>(
            sessions, body, grpc_clients, path, std::move(callback));
    }
    if (path == "/service/file/put_single_file") {
        return AuthenticateAndInjectUserAsync<zchat::PutSingleFileReq,
                                              zchat::PutSingleFileRsp>(
            sessions, body, grpc_clients, path, std::move(callback));
    }
    if (path == "/service/file/put_multi_file") {
        return AuthenticateAndInjectUserAsync<zchat::PutMultiFileReq,
                                              zchat::PutMultiFileRsp>(
            sessions, body, grpc_clients, path, std::move(callback));
    }
    if (path == "/service/speech/recognition") {
        return AuthenticateAndInjectUserAsync<zchat::SpeechRecognitionReq,
                                              zchat::SpeechRecognitionRsp>(
            sessions, body, grpc_clients, path, std::move(callback));
    }
    grpc_clients.Forward(path, body, std::move(callback));
}

} // namespace

GatewayController::GatewayController(std::shared_ptr<GatewayContext> context)
    : context_(std::move(context)) {}

void GatewayController::RegisterRoutes() {
    drogon::app().registerHandler(
        "/ping",
        [](const drogon::HttpRequestPtr &,
           std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
            callback(TextResponse("pong"));
        },
        {drogon::Get});

    RegisterForwardPost("/service/user/get_phone_verify_code", "user");
    RegisterForwardPost("/service/user/username_register", "user");
    RegisterForwardPost("/service/user/username_login", "user");
    RegisterForwardPost("/service/user/phone_register", "user");
    RegisterForwardPost("/service/user/phone_login", "user");
    RegisterForwardPost("/service/user/get_user_info", "user");
    RegisterForwardPost("/service/user/set_avatar", "user");
    RegisterForwardPost("/service/user/set_nickname", "user");
    RegisterForwardPost("/service/user/set_description", "user");
    RegisterForwardPost("/service/user/set_phone", "user");

    RegisterForwardPost("/service/friend/get_friend_list", "friend");
    RegisterForwardPost("/service/friend/get_chat_session_list", "friend");
    RegisterForwardPost("/service/friend/get_pending_friend_events", "friend");
    RegisterForwardPost("/service/friend/remove_friend", "friend");
    RegisterForwardPost("/service/friend/add_friend_apply", "friend");
    RegisterForwardPost("/service/friend/add_friend_process", "friend");
    RegisterForwardPost("/service/friend/create_chat_session", "friend");
    RegisterForwardPost("/service/friend/get_chat_session_member", "friend");
    RegisterForwardPost("/service/friend/search_friend", "friend");

    RegisterForwardPost("/service/message_storage/get_recent", "message");
    RegisterForwardPost("/service/message_storage/get_history", "message");
    RegisterForwardPost("/service/message_storage/search_history", "message");
    RegisterForwardPost("/service/message_transmit/new_message", "transmite");
    RegisterForwardPost("/service/file/get_single_file", "file");
    RegisterForwardPost("/service/file/get_multi_file", "file");
    RegisterForwardPost("/service/file/put_single_file", "file");
    RegisterForwardPost("/service/file/put_multi_file", "file");
    RegisterForwardPost("/service/speech/recognition", "speech");
}

void GatewayController::RegisterForwardPost(const std::string &path,
                                            const std::string &service_name) {
    drogon::app().registerHandler(
        path,
        [this, path, service_name](
            const drogon::HttpRequestPtr &request,
            std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
            ZCHAT_LOG_DEBUG("forward http path={} service={} body={}B", path,
                            service_name, request->body().size());
            PrepareForwardBodyAsync(
                context_->sessions(), path, std::string(request->body()),
                context_->grpc_clients(), std::move(callback));
        },
        {drogon::Post});
}

} // namespace zchat
