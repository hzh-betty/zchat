#include "gateway/gateway_controller.h"

#include <functional>
#include <string>
#include <utility>

#include "common/logger.h"
#include "common/protobuf_http.h"
#include "file.pb.h"
#include "friend.pb.h"
#include "message.pb.h"
#include "speech.pb.h"
#include "transmite.pb.h"
#include "user.pb.h"

namespace zchat {
namespace {

template <typename Response>
drogon::HttpResponsePtr GatewayErrorResponse(const std::string &message) {
    Response response;
    response.set_success(false);
    response.set_errmsg(message);
    return ProtobufResponse(response);
}

template <typename Request, typename Response>
bool AuthenticateAndInjectUser(SessionStore &sessions, const std::string &body,
                               std::string *forward_body,
                               std::function<void(
                                   const drogon::HttpResponsePtr &)> &callback) {
    Request request;
    if (!request.ParseFromString(body)) {
        ZCHAT_LOG_WARN("gateway auth protobuf parse failed, body size={}B",
                       body.size());
        callback(GatewayErrorResponse<Response>("请求正文反序列化失败"));
        return false;
    }
    auto user_id = sessions.GetUserId(request.session_id());
    if (!user_id.ok()) {
        ZCHAT_LOG_WARN("gateway auth redis failed: {}", user_id.error().message);
        callback(GatewayErrorResponse<Response>(user_id.error().message));
        return false;
    }
    if (!user_id.value().has_value()) {
        ZCHAT_LOG_WARN("gateway auth rejected: invalid session={}",
                       request.session_id());
        callback(GatewayErrorResponse<Response>("登录会话已失效"));
        return false;
    }
    request.set_user_id(user_id.value().value());
    *forward_body = request.SerializeAsString();
    return true;
}

bool IsPublicPath(const std::string &path) {
    return path == "/service/user/get_phone_verify_code" ||
           path == "/service/user/username_register" ||
           path == "/service/user/username_login" ||
           path == "/service/user/phone_register" ||
           path == "/service/user/phone_login";
}

bool PrepareForwardBody(
    SessionStore &sessions, const std::string &path, const std::string &body,
    std::string *forward_body,
    std::function<void(const drogon::HttpResponsePtr &)> &callback) {
    *forward_body = body;
    if (IsPublicPath(path)) {
        return true;
    }
    if (path == "/service/user/get_user_info") {
        return AuthenticateAndInjectUser<zchat::GetUserInfoReq,
                                         zchat::GetUserInfoRsp>(
            sessions, body, forward_body, callback);
    }
    if (path == "/service/user/set_avatar") {
        return AuthenticateAndInjectUser<zchat::SetUserAvatarReq,
                                         zchat::SetUserAvatarRsp>(
            sessions, body, forward_body, callback);
    }
    if (path == "/service/user/set_nickname") {
        return AuthenticateAndInjectUser<zchat::SetUserNicknameReq,
                                         zchat::SetUserNicknameRsp>(
            sessions, body, forward_body, callback);
    }
    if (path == "/service/user/set_description") {
        return AuthenticateAndInjectUser<zchat::SetUserDescriptionReq,
                                         zchat::SetUserDescriptionRsp>(
            sessions, body, forward_body, callback);
    }
    if (path == "/service/user/set_phone") {
        return AuthenticateAndInjectUser<zchat::SetUserPhoneNumberReq,
                                         zchat::SetUserPhoneNumberRsp>(
            sessions, body, forward_body, callback);
    }
    if (path == "/service/friend/get_friend_list") {
        return AuthenticateAndInjectUser<zchat::GetFriendListReq,
                                         zchat::GetFriendListRsp>(
            sessions, body, forward_body, callback);
    }
    if (path == "/service/friend/get_chat_session_list") {
        return AuthenticateAndInjectUser<zchat::GetChatSessionListReq,
                                         zchat::GetChatSessionListRsp>(
            sessions, body, forward_body, callback);
    }
    if (path == "/service/friend/get_pending_friend_events") {
        return AuthenticateAndInjectUser<zchat::GetPendingFriendEventListReq,
                                         zchat::GetPendingFriendEventListRsp>(
            sessions, body, forward_body, callback);
    }
    if (path == "/service/friend/remove_friend") {
        return AuthenticateAndInjectUser<zchat::FriendRemoveReq,
                                         zchat::FriendRemoveRsp>(
            sessions, body, forward_body, callback);
    }
    if (path == "/service/friend/add_friend_apply") {
        return AuthenticateAndInjectUser<zchat::FriendAddReq,
                                         zchat::FriendAddRsp>(
            sessions, body, forward_body, callback);
    }
    if (path == "/service/friend/add_friend_process") {
        return AuthenticateAndInjectUser<zchat::FriendAddProcessReq,
                                         zchat::FriendAddProcessRsp>(
            sessions, body, forward_body, callback);
    }
    if (path == "/service/friend/create_chat_session") {
        return AuthenticateAndInjectUser<zchat::ChatSessionCreateReq,
                                         zchat::ChatSessionCreateRsp>(
            sessions, body, forward_body, callback);
    }
    if (path == "/service/friend/get_chat_session_member") {
        return AuthenticateAndInjectUser<zchat::GetChatSessionMemberReq,
                                         zchat::GetChatSessionMemberRsp>(
            sessions, body, forward_body, callback);
    }
    if (path == "/service/friend/search_friend") {
        return AuthenticateAndInjectUser<zchat::FriendSearchReq,
                                         zchat::FriendSearchRsp>(
            sessions, body, forward_body, callback);
    }
    if (path == "/service/message_storage/get_recent") {
        return AuthenticateAndInjectUser<zchat::GetRecentMsgReq,
                                         zchat::GetRecentMsgRsp>(
            sessions, body, forward_body, callback);
    }
    if (path == "/service/message_storage/get_history") {
        return AuthenticateAndInjectUser<zchat::GetHistoryMsgReq,
                                         zchat::GetHistoryMsgRsp>(
            sessions, body, forward_body, callback);
    }
    if (path == "/service/message_storage/search_history") {
        return AuthenticateAndInjectUser<zchat::MsgSearchReq,
                                         zchat::MsgSearchRsp>(
            sessions, body, forward_body, callback);
    }
    if (path == "/service/message_transmit/new_message") {
        return AuthenticateAndInjectUser<zchat::NewMessageReq,
                                         zchat::NewMessageRsp>(
            sessions, body, forward_body, callback);
    }
    if (path == "/service/file/get_single_file") {
        return AuthenticateAndInjectUser<zchat::GetSingleFileReq,
                                         zchat::GetSingleFileRsp>(
            sessions, body, forward_body, callback);
    }
    if (path == "/service/file/get_multi_file") {
        return AuthenticateAndInjectUser<zchat::GetMultiFileReq,
                                         zchat::GetMultiFileRsp>(
            sessions, body, forward_body, callback);
    }
    if (path == "/service/file/put_single_file") {
        return AuthenticateAndInjectUser<zchat::PutSingleFileReq,
                                         zchat::PutSingleFileRsp>(
            sessions, body, forward_body, callback);
    }
    if (path == "/service/file/put_multi_file") {
        return AuthenticateAndInjectUser<zchat::PutMultiFileReq,
                                         zchat::PutMultiFileRsp>(
            sessions, body, forward_body, callback);
    }
    if (path == "/service/speech/recognition") {
        return AuthenticateAndInjectUser<zchat::SpeechRecognitionReq,
                                         zchat::SpeechRecognitionRsp>(
            sessions, body, forward_body, callback);
    }
    return true;
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
            std::string forward_body;
            if (!PrepareForwardBody(context_->sessions(), path,
                                    std::string(request->body()),
                                    &forward_body, callback)) {
                return;
            }
            ZCHAT_LOG_DEBUG("forward http path={} service={} body={}B", path,
                            service_name, forward_body.size());
            context_->grpc_clients().Forward(path, forward_body,
                                             std::move(callback));
        },
        {drogon::Post});
}

} // namespace zchat
