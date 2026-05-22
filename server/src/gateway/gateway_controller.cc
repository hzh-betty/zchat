#include "gateway/gateway_controller.h"

#include <functional>
#include <string>
#include <utility>

#include "common/logger.h"
#include "common/protobuf_http.h"

namespace zchat {

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
            context_->grpc_clients().Forward(path, std::string(request->body()),
                                             std::move(callback));
        },
        {drogon::Post});
}

} // namespace zchat
