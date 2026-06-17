#include "gateway/grpc_service_clients.h"

#include <chrono>

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

template <typename Request, typename Response, typename Rpc>
void CallUnaryGrpc(
    const std::string &body,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback, Rpc rpc,
    std::chrono::seconds deadline) {
    Request request;
    if (!request.ParseFromString(body)) {
        ZCHAT_LOG_WARN("grpc protobuf parse failed, body size={}B",
                       body.size());
        callback(GrpcErrorResponse<Response>(
            common_errors::RequestBodyParseFailed()));
        return;
    }
    Response response;
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + deadline);
    const grpc::Status status = rpc(&context, request, &response);
    if (!status.ok()) {
        callback(GrpcErrorResponse<Response>(
            AppError::WithCode(ErrorCode::kExternalServiceError,
                               "grpc request failed")
                .WithDetail(status.error_message())));
        return;
    }
    callback(ProtobufResponse(response));
}

template <typename Service, typename Request, typename Response,
          typename Method>
void CallStub(GrpcServiceClients &clients, const std::string &service_name,
              Method method, const std::string &body,
              std::function<void(const drogon::HttpResponsePtr &)> &&callback,
              std::chrono::seconds deadline = std::chrono::seconds(5)) {
    auto endpoint = clients.discovery().Endpoint(service_name);
    if (!endpoint.ok()) {
        callback(GrpcErrorResponse<Response>(endpoint.error()));
        return;
    }
    auto stub = Service::NewStub(clients.GetOrCreateChannel(endpoint.value()));
    CallUnaryGrpc<Request, Response>(
        body, std::move(callback),
        [stub = std::move(stub), method](grpc::ClientContext *context,
                                         const Request &request,
                                         Response *response) {
            return (stub.get()->*method)(context, request, response);
        },
        deadline);
}

void CallTransmite(
    GrpcServiceClients &clients, const std::string &body,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    zchat::NewMessageReq request;
    if (!request.ParseFromString(body)) {
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
    auto stub = zchat::MsgTransmitService::NewStub(
        clients.GetOrCreateChannel(endpoint.value()));
    zchat::GetTransmitTargetRsp target_response;
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() +
                         std::chrono::seconds(5));
    const grpc::Status status =
        stub->GetTransmitTarget(&context, request, &target_response);
    if (!status.ok()) {
        ZCHAT_LOG_ERROR(
            "GetTransmitTarget rpc failed: error_code={}, error_message={}",
            static_cast<int>(status.error_code()), status.error_message());
        callback(GrpcErrorResponse<zchat::NewMessageRsp>(
            AppError::WithCode(ErrorCode::kExternalServiceError,
                               "grpc request failed")
                .WithDetail(status.error_message())));
        return;
    }
    zchat::NewMessageRsp response;
    response.set_request_id(target_response.request_id());
    response.set_success(target_response.success());
    response.set_errmsg(target_response.errmsg());
    callback(ProtobufResponse(response));
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
        return CallStub<zchat::UserService, zchat::PhoneVerifyCodeReq,
                        zchat::PhoneVerifyCodeRsp>(
            *this, "user_service",
            &zchat::UserService::Stub::GetPhoneVerifyCode, body,
            std::move(callback));
    }
    if (path == "/service/user/username_register") {
        return CallStub<zchat::UserService, zchat::UserRegisterReq,
                        zchat::UserRegisterRsp>(
            *this, "user_service", &zchat::UserService::Stub::UserRegister,
            body, std::move(callback));
    }
    if (path == "/service/user/username_login") {
        return CallStub<zchat::UserService, zchat::UserLoginReq,
                        zchat::UserLoginRsp>(
            *this, "user_service", &zchat::UserService::Stub::UserLogin, body,
            std::move(callback));
    }
    if (path == "/service/user/phone_register") {
        return CallStub<zchat::UserService, zchat::PhoneRegisterReq,
                        zchat::PhoneRegisterRsp>(
            *this, "user_service", &zchat::UserService::Stub::PhoneRegister,
            body, std::move(callback));
    }
    if (path == "/service/user/phone_login") {
        return CallStub<zchat::UserService, zchat::PhoneLoginReq,
                        zchat::PhoneLoginRsp>(
            *this, "user_service", &zchat::UserService::Stub::PhoneLogin, body,
            std::move(callback));
    }
    if (path == "/service/user/get_user_info") {
        return CallStub<zchat::UserService, zchat::GetUserInfoReq,
                        zchat::GetUserInfoRsp>(
            *this, "user_service", &zchat::UserService::Stub::GetUserInfo, body,
            std::move(callback));
    }
    if (path == "/service/user/set_avatar") {
        return CallStub<zchat::UserService, zchat::SetUserAvatarReq,
                        zchat::SetUserAvatarRsp>(
            *this, "user_service", &zchat::UserService::Stub::SetUserAvatar,
            body, std::move(callback));
    }
    if (path == "/service/user/set_nickname") {
        return CallStub<zchat::UserService, zchat::SetUserNicknameReq,
                        zchat::SetUserNicknameRsp>(
            *this, "user_service", &zchat::UserService::Stub::SetUserNickname,
            body, std::move(callback));
    }
    if (path == "/service/user/set_description") {
        return CallStub<zchat::UserService, zchat::SetUserDescriptionReq,
                        zchat::SetUserDescriptionRsp>(
            *this, "user_service",
            &zchat::UserService::Stub::SetUserDescription, body,
            std::move(callback));
    }
    if (path == "/service/user/set_phone") {
        return CallStub<zchat::UserService, zchat::SetUserPhoneNumberReq,
                        zchat::SetUserPhoneNumberRsp>(
            *this, "user_service",
            &zchat::UserService::Stub::SetUserPhoneNumber, body,
            std::move(callback));
    }
    if (path == "/service/friend/get_friend_list") {
        return CallStub<zchat::FriendService, zchat::GetFriendListReq,
                        zchat::GetFriendListRsp>(
            *this, "friend_service", &zchat::FriendService::Stub::GetFriendList,
            body, std::move(callback));
    }
    if (path == "/service/friend/get_chat_session_list") {
        return CallStub<zchat::FriendService, zchat::GetChatSessionListReq,
                        zchat::GetChatSessionListRsp>(
            *this, "friend_service",
            &zchat::FriendService::Stub::GetChatSessionList, body,
            std::move(callback));
    }
    if (path == "/service/friend/get_pending_friend_events") {
        return CallStub<zchat::FriendService,
                        zchat::GetPendingFriendEventListReq,
                        zchat::GetPendingFriendEventListRsp>(
            *this, "friend_service",
            &zchat::FriendService::Stub::GetPendingFriendEventList, body,
            std::move(callback));
    }
    if (path == "/service/friend/remove_friend") {
        return CallStub<zchat::FriendService, zchat::FriendRemoveReq,
                        zchat::FriendRemoveRsp>(
            *this, "friend_service", &zchat::FriendService::Stub::FriendRemove,
            body, std::move(callback));
    }
    if (path == "/service/friend/add_friend_apply") {
        return CallStub<zchat::FriendService, zchat::FriendAddReq,
                        zchat::FriendAddRsp>(
            *this, "friend_service", &zchat::FriendService::Stub::FriendAdd,
            body, std::move(callback));
    }
    if (path == "/service/friend/add_friend_process") {
        return CallStub<zchat::FriendService, zchat::FriendAddProcessReq,
                        zchat::FriendAddProcessRsp>(
            *this, "friend_service",
            &zchat::FriendService::Stub::FriendAddProcess, body,
            std::move(callback));
    }
    if (path == "/service/friend/create_chat_session") {
        return CallStub<zchat::FriendService, zchat::ChatSessionCreateReq,
                        zchat::ChatSessionCreateRsp>(
            *this, "friend_service",
            &zchat::FriendService::Stub::ChatSessionCreate, body,
            std::move(callback));
    }
    if (path == "/service/friend/get_chat_session_member") {
        return CallStub<zchat::FriendService, zchat::GetChatSessionMemberReq,
                        zchat::GetChatSessionMemberRsp>(
            *this, "friend_service",
            &zchat::FriendService::Stub::GetChatSessionMember, body,
            std::move(callback));
    }
    if (path == "/service/friend/search_friend") {
        return CallStub<zchat::FriendService, zchat::FriendSearchReq,
                        zchat::FriendSearchRsp>(
            *this, "friend_service", &zchat::FriendService::Stub::FriendSearch,
            body, std::move(callback));
    }
    if (path == "/service/message_storage/get_recent") {
        return CallStub<zchat::MsgStorageService, zchat::GetRecentMsgReq,
                        zchat::GetRecentMsgRsp>(
            *this, "message_service",
            &zchat::MsgStorageService::Stub::GetRecentMsg, body,
            std::move(callback));
    }
    if (path == "/service/message_storage/get_history") {
        return CallStub<zchat::MsgStorageService, zchat::GetHistoryMsgReq,
                        zchat::GetHistoryMsgRsp>(
            *this, "message_service",
            &zchat::MsgStorageService::Stub::GetHistoryMsg, body,
            std::move(callback));
    }
    if (path == "/service/message_storage/search_history") {
        return CallStub<zchat::MsgStorageService, zchat::MsgSearchReq,
                        zchat::MsgSearchRsp>(
            *this, "message_service",
            &zchat::MsgStorageService::Stub::MsgSearch, body,
            std::move(callback));
    }
    if (path == "/service/message_transmit/new_message") {
        return CallTransmite(*this, body, std::move(callback));
    }
    if (path == "/service/file/get_single_file") {
        return CallStub<zchat::FileService, zchat::GetSingleFileReq,
                        zchat::GetSingleFileRsp>(
            *this, "file_service", &zchat::FileService::Stub::GetSingleFile,
            body, std::move(callback), std::chrono::seconds(30));
    }
    if (path == "/service/file/get_multi_file") {
        return CallStub<zchat::FileService, zchat::GetMultiFileReq,
                        zchat::GetMultiFileRsp>(
            *this, "file_service", &zchat::FileService::Stub::GetMultiFile,
            body, std::move(callback), std::chrono::seconds(30));
    }
    if (path == "/service/file/put_single_file") {
        return CallStub<zchat::FileService, zchat::PutSingleFileReq,
                        zchat::PutSingleFileRsp>(
            *this, "file_service", &zchat::FileService::Stub::PutSingleFile,
            body, std::move(callback), std::chrono::seconds(30));
    }
    if (path == "/service/file/put_multi_file") {
        return CallStub<zchat::FileService, zchat::PutMultiFileReq,
                        zchat::PutMultiFileRsp>(
            *this, "file_service", &zchat::FileService::Stub::PutMultiFile,
            body, std::move(callback), std::chrono::seconds(30));
    }
    if (path == "/service/speech/recognition") {
        return CallStub<zchat::SpeechService, zchat::SpeechRecognitionReq,
                        zchat::SpeechRecognitionRsp>(
            *this, "speech_service",
            &zchat::SpeechService::Stub::SpeechRecognition, body,
            std::move(callback));
    }
    ZCHAT_LOG_WARN("unknown service path: {}", path);
    callback(GrpcErrorResponse<zchat::NewMessageRsp>(
        common_errors::UnknownServicePath()));
}

} // namespace zchat
