#include "gateway/grpc_service_clients.h"

#include <chrono>
#include <memory>
#include <utility>

#include <grpcpp/grpcpp.h>

#include "common/common_errors.h"
#include "common/logger.h"
#include "common/protobuf_http.h"
#include "common/result.h"

namespace zchat {
namespace {

template <typename Response>
drogon::HttpResponsePtr GrpcErrorResponse(const AppError &error) {
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
        callback(GrpcErrorResponse<Response>(
            common_errors::RequestBodyParseFailed()));
        return;
    }
    auto endpoint = clients.discovery().Endpoint(service_name);
    if (!endpoint.ok()) {
        callback(GrpcErrorResponse<Response>(endpoint.error()));
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
                       callback(GrpcErrorResponse<Response>(
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
        callback(GrpcErrorResponse<zchat::NewMessageRsp>(
            common_errors::RequestBodyParseFailed()));
        return;
    }
    auto endpoint = clients.discovery().Endpoint("transmite_service");
    if (!endpoint.ok()) {
        callback(GrpcErrorResponse<zchat::NewMessageRsp>(endpoint.error()));
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
                callback(GrpcErrorResponse<zchat::NewMessageRsp>(
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

} // namespace

GrpcServiceClients::GrpcServiceClients(const AppConfig &config)
    : discovery_(config.etcd) {}

std::shared_ptr<grpc::Channel>
GrpcServiceClients::GetOrCreateChannel(const std::string &endpoint) {
    std::lock_guard<std::mutex> lock(channel_mutex_);
    auto it = channels_.find(endpoint);
    if (it != channels_.end()) {
        return it->second;
    }
    auto channel =
        grpc::CreateChannel(endpoint, grpc::InsecureChannelCredentials());
    channels_[endpoint] = channel;
    return channel;
}

void GrpcServiceClients::Forward(
    const std::string &path, const std::string &body,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    if (path == "/service/user/get_phone_verify_code") {
        return CallStubAsync<zchat::UserService, zchat::PhoneVerifyCodeReq,
                             zchat::PhoneVerifyCodeRsp>(
            *this, "user_service",
            [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                s->async()->GetPhoneVerifyCode(c, q, r, std::move(cb));
            },
            body, std::move(callback));
    }
    if (path == "/service/user/username_register") {
        return CallStubAsync<zchat::UserService, zchat::UserRegisterReq,
                             zchat::UserRegisterRsp>(
            *this, "user_service",
            [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                s->async()->UserRegister(c, q, r, std::move(cb));
            },
            body, std::move(callback));
    }
    if (path == "/service/user/username_login") {
        return CallStubAsync<zchat::UserService, zchat::UserLoginReq,
                             zchat::UserLoginRsp>(
            *this, "user_service",
            [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                s->async()->UserLogin(c, q, r, std::move(cb));
            },
            body, std::move(callback));
    }
    if (path == "/service/user/phone_register") {
        return CallStubAsync<zchat::UserService, zchat::PhoneRegisterReq,
                             zchat::PhoneRegisterRsp>(
            *this, "user_service",
            [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                s->async()->PhoneRegister(c, q, r, std::move(cb));
            },
            body, std::move(callback));
    }
    if (path == "/service/user/phone_login") {
        return CallStubAsync<zchat::UserService, zchat::PhoneLoginReq,
                             zchat::PhoneLoginRsp>(
            *this, "user_service",
            [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                s->async()->PhoneLogin(c, q, r, std::move(cb));
            },
            body, std::move(callback));
    }
    if (path == "/service/user/get_user_info") {
        return CallStubAsync<zchat::UserService, zchat::GetUserInfoReq,
                             zchat::GetUserInfoRsp>(
            *this, "user_service",
            [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                s->async()->GetUserInfo(c, q, r, std::move(cb));
            },
            body, std::move(callback));
    }
    if (path == "/service/user/set_avatar") {
        return CallStubAsync<zchat::UserService, zchat::SetUserAvatarReq,
                             zchat::SetUserAvatarRsp>(
            *this, "user_service",
            [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                s->async()->SetUserAvatar(c, q, r, std::move(cb));
            },
            body, std::move(callback));
    }
    if (path == "/service/user/set_nickname") {
        return CallStubAsync<zchat::UserService, zchat::SetUserNicknameReq,
                             zchat::SetUserNicknameRsp>(
            *this, "user_service",
            [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                s->async()->SetUserNickname(c, q, r, std::move(cb));
            },
            body, std::move(callback));
    }
    if (path == "/service/user/set_description") {
        return CallStubAsync<zchat::UserService, zchat::SetUserDescriptionReq,
                             zchat::SetUserDescriptionRsp>(
            *this, "user_service",
            [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                s->async()->SetUserDescription(c, q, r, std::move(cb));
            },
            body, std::move(callback));
    }
    if (path == "/service/user/set_phone") {
        return CallStubAsync<zchat::UserService, zchat::SetUserPhoneNumberReq,
                             zchat::SetUserPhoneNumberRsp>(
            *this, "user_service",
            [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                s->async()->SetUserPhoneNumber(c, q, r, std::move(cb));
            },
            body, std::move(callback));
    }
    if (path == "/service/friend/get_friend_list") {
        return CallStubAsync<zchat::FriendService, zchat::GetFriendListReq,
                             zchat::GetFriendListRsp>(
            *this, "friend_service",
            [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                s->async()->GetFriendList(c, q, r, std::move(cb));
            },
            body, std::move(callback));
    }
    if (path == "/service/friend/get_chat_session_list") {
        return CallStubAsync<zchat::FriendService, zchat::GetChatSessionListReq,
                             zchat::GetChatSessionListRsp>(
            *this, "friend_service",
            [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                s->async()->GetChatSessionList(c, q, r, std::move(cb));
            },
            body, std::move(callback));
    }
    if (path == "/service/friend/get_pending_friend_events") {
        return CallStubAsync<zchat::FriendService,
                             zchat::GetPendingFriendEventListReq,
                             zchat::GetPendingFriendEventListRsp>(
            *this, "friend_service",
            [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                s->async()->GetPendingFriendEventList(c, q, r, std::move(cb));
            },
            body, std::move(callback));
    }
    if (path == "/service/friend/remove_friend") {
        return CallStubAsync<zchat::FriendService, zchat::FriendRemoveReq,
                             zchat::FriendRemoveRsp>(
            *this, "friend_service",
            [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                s->async()->FriendRemove(c, q, r, std::move(cb));
            },
            body, std::move(callback));
    }
    if (path == "/service/friend/add_friend_apply") {
        return CallStubAsync<zchat::FriendService, zchat::FriendAddReq,
                             zchat::FriendAddRsp>(
            *this, "friend_service",
            [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                s->async()->FriendAdd(c, q, r, std::move(cb));
            },
            body, std::move(callback));
    }
    if (path == "/service/friend/add_friend_process") {
        return CallStubAsync<zchat::FriendService, zchat::FriendAddProcessReq,
                             zchat::FriendAddProcessRsp>(
            *this, "friend_service",
            [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                s->async()->FriendAddProcess(c, q, r, std::move(cb));
            },
            body, std::move(callback));
    }
    if (path == "/service/friend/create_chat_session") {
        return CallStubAsync<zchat::FriendService, zchat::ChatSessionCreateReq,
                             zchat::ChatSessionCreateRsp>(
            *this, "friend_service",
            [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                s->async()->ChatSessionCreate(c, q, r, std::move(cb));
            },
            body, std::move(callback));
    }
    if (path == "/service/friend/get_chat_session_member") {
        return CallStubAsync<zchat::FriendService,
                             zchat::GetChatSessionMemberReq,
                             zchat::GetChatSessionMemberRsp>(
            *this, "friend_service",
            [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                s->async()->GetChatSessionMember(c, q, r, std::move(cb));
            },
            body, std::move(callback));
    }
    if (path == "/service/friend/search_friend") {
        return CallStubAsync<zchat::FriendService, zchat::FriendSearchReq,
                             zchat::FriendSearchRsp>(
            *this, "friend_service",
            [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                s->async()->FriendSearch(c, q, r, std::move(cb));
            },
            body, std::move(callback));
    }
    if (path == "/service/message_storage/get_recent") {
        return CallStubAsync<zchat::MsgStorageService, zchat::GetRecentMsgReq,
                             zchat::GetRecentMsgRsp>(
            *this, "message_service",
            [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                s->async()->GetRecentMsg(c, q, r, std::move(cb));
            },
            body, std::move(callback));
    }
    if (path == "/service/message_storage/get_history") {
        return CallStubAsync<zchat::MsgStorageService, zchat::GetHistoryMsgReq,
                             zchat::GetHistoryMsgRsp>(
            *this, "message_service",
            [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                s->async()->GetHistoryMsg(c, q, r, std::move(cb));
            },
            body, std::move(callback));
    }
    if (path == "/service/message_storage/search_history") {
        return CallStubAsync<zchat::MsgStorageService, zchat::MsgSearchReq,
                             zchat::MsgSearchRsp>(
            *this, "message_service",
            [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                s->async()->MsgSearch(c, q, r, std::move(cb));
            },
            body, std::move(callback));
    }
    if (path == "/service/message_transmit/new_message") {
        return CallTransmiteAsync(*this, body, std::move(callback));
    }
    if (path == "/service/file/get_single_file") {
        return CallStubAsync<zchat::FileService, zchat::GetSingleFileReq,
                             zchat::GetSingleFileRsp>(
            *this, "file_service",
            [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                s->async()->GetSingleFile(c, q, r, std::move(cb));
            },
            body, std::move(callback), std::chrono::seconds(30));
    }
    if (path == "/service/file/get_multi_file") {
        return CallStubAsync<zchat::FileService, zchat::GetMultiFileReq,
                             zchat::GetMultiFileRsp>(
            *this, "file_service",
            [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                s->async()->GetMultiFile(c, q, r, std::move(cb));
            },
            body, std::move(callback), std::chrono::seconds(30));
    }
    if (path == "/service/file/put_single_file") {
        return CallStubAsync<zchat::FileService, zchat::PutSingleFileReq,
                             zchat::PutSingleFileRsp>(
            *this, "file_service",
            [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                s->async()->PutSingleFile(c, q, r, std::move(cb));
            },
            body, std::move(callback), std::chrono::seconds(30));
    }
    if (path == "/service/file/put_multi_file") {
        return CallStubAsync<zchat::FileService, zchat::PutMultiFileReq,
                             zchat::PutMultiFileRsp>(
            *this, "file_service",
            [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                s->async()->PutMultiFile(c, q, r, std::move(cb));
            },
            body, std::move(callback), std::chrono::seconds(30));
    }
    if (path == "/service/speech/recognition") {
        return CallStubAsync<zchat::SpeechService, zchat::SpeechRecognitionReq,
                             zchat::SpeechRecognitionRsp>(
            *this, "speech_service",
            [](auto *s, auto *c, auto *q, auto *r, auto &&cb) {
                s->async()->SpeechRecognition(c, q, r, std::move(cb));
            },
            body, std::move(callback));
    }
    ZCHAT_LOG_WARN("unknown service path: {}", path);
    callback(GrpcErrorResponse<zchat::NewMessageRsp>(
        common_errors::UnknownServicePath()));
}

} // namespace zchat
