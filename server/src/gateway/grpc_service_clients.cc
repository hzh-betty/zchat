#include "gateway/grpc_service_clients.h"

#include <grpcpp/grpcpp.h>

#include "common/logger.h"
#include "common/protobuf_http.h"

namespace zchat {
namespace {

std::shared_ptr<grpc::Channel> LocalChannel(int port) {
    return grpc::CreateChannel("127.0.0.1:" + std::to_string(port),
                               grpc::InsecureChannelCredentials());
}

template <typename Response>
drogon::HttpResponsePtr GrpcErrorResponse(const std::string &message) {
    Response response;
    response.set_success(false);
    response.set_errmsg(message);
    return ProtobufResponse(response);
}

template <typename Request, typename Response, typename Rpc>
void CallUnaryGrpc(
    const std::string &body,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback, Rpc rpc) {
    Request request;
    if (!request.ParseFromString(body)) {
        ZCHAT_LOG_WARN("grpc protobuf parse failed, body size={}B", body.size());
        callback(GrpcErrorResponse<Response>("请求正文反序列化失败"));
        return;
    }
    Response response;
    grpc::ClientContext context;
    const grpc::Status status = rpc(&context, request, &response);
    if (!status.ok()) {
        callback(GrpcErrorResponse<Response>(status.error_message()));
        return;
    }
    callback(ProtobufResponse(response));
}

template <typename Stub, typename Request, typename Response, typename Method>
void CallStub(Stub *stub, Method method, const std::string &body,
              std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    CallUnaryGrpc<Request, Response>(
        body, std::move(callback),
        [stub, method](grpc::ClientContext *context, const Request &request,
                       Response *response) {
            return (stub->*method)(context, request, response);
        });
}

void CallTransmite(
    zchat::MsgTransmitService::Stub *stub, const std::string &body,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    zchat::NewMessageReq request;
    if (!request.ParseFromString(body)) {
        ZCHAT_LOG_WARN("grpc protobuf parse failed, body size={}B", body.size());
        callback(
            GrpcErrorResponse<zchat::NewMessageRsp>("请求正文反序列化失败"));
        return;
    }
    zchat::GetTransmitTargetRsp target_response;
    grpc::ClientContext context;
    const grpc::Status status =
        stub->GetTransmitTarget(&context, request, &target_response);
    if (!status.ok()) {
        ZCHAT_LOG_ERROR("GetTransmitTarget rpc failed: error_code={}, error_message={}",
                        status.error_code(), status.error_message());
        callback(
            GrpcErrorResponse<zchat::NewMessageRsp>(status.error_message()));
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
    : user_(zchat::UserService::NewStub(LocalChannel(config.services.user))),
      friend_(zchat::FriendService::NewStub(
          LocalChannel(config.services.friend_service))),
      message_(zchat::MsgStorageService::NewStub(
          LocalChannel(config.services.message))),
      transmite_(zchat::MsgTransmitService::NewStub(
          LocalChannel(config.services.transmite))),
      file_(zchat::FileService::NewStub(LocalChannel(config.services.file))),
      speech_(
          zchat::SpeechService::NewStub(LocalChannel(config.services.speech))) {
}

void GrpcServiceClients::Forward(
    const std::string &path, const std::string &body,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    if (path == "/service/user/get_phone_verify_code") {
        return CallStub<zchat::UserService::Stub, zchat::PhoneVerifyCodeReq,
                        zchat::PhoneVerifyCodeRsp>(
            user_.get(), &zchat::UserService::Stub::GetPhoneVerifyCode, body,
            std::move(callback));
    }
    if (path == "/service/user/username_register") {
        return CallStub<zchat::UserService::Stub, zchat::UserRegisterReq,
                        zchat::UserRegisterRsp>(
            user_.get(), &zchat::UserService::Stub::UserRegister, body,
            std::move(callback));
    }
    if (path == "/service/user/username_login") {
        return CallStub<zchat::UserService::Stub, zchat::UserLoginReq,
                        zchat::UserLoginRsp>(
            user_.get(), &zchat::UserService::Stub::UserLogin, body,
            std::move(callback));
    }
    if (path == "/service/user/phone_register") {
        return CallStub<zchat::UserService::Stub, zchat::PhoneRegisterReq,
                        zchat::PhoneRegisterRsp>(
            user_.get(), &zchat::UserService::Stub::PhoneRegister, body,
            std::move(callback));
    }
    if (path == "/service/user/phone_login") {
        return CallStub<zchat::UserService::Stub, zchat::PhoneLoginReq,
                        zchat::PhoneLoginRsp>(
            user_.get(), &zchat::UserService::Stub::PhoneLogin, body,
            std::move(callback));
    }
    if (path == "/service/user/get_user_info") {
        return CallStub<zchat::UserService::Stub, zchat::GetUserInfoReq,
                        zchat::GetUserInfoRsp>(
            user_.get(), &zchat::UserService::Stub::GetUserInfo, body,
            std::move(callback));
    }
    if (path == "/service/user/set_avatar") {
        return CallStub<zchat::UserService::Stub, zchat::SetUserAvatarReq,
                        zchat::SetUserAvatarRsp>(
            user_.get(), &zchat::UserService::Stub::SetUserAvatar, body,
            std::move(callback));
    }
    if (path == "/service/user/set_nickname") {
        return CallStub<zchat::UserService::Stub, zchat::SetUserNicknameReq,
                        zchat::SetUserNicknameRsp>(
            user_.get(), &zchat::UserService::Stub::SetUserNickname, body,
            std::move(callback));
    }
    if (path == "/service/user/set_description") {
        return CallStub<zchat::UserService::Stub, zchat::SetUserDescriptionReq,
                        zchat::SetUserDescriptionRsp>(
            user_.get(), &zchat::UserService::Stub::SetUserDescription, body,
            std::move(callback));
    }
    if (path == "/service/user/set_phone") {
        return CallStub<zchat::UserService::Stub, zchat::SetUserPhoneNumberReq,
                        zchat::SetUserPhoneNumberRsp>(
            user_.get(), &zchat::UserService::Stub::SetUserPhoneNumber, body,
            std::move(callback));
    }
    if (path == "/service/friend/get_friend_list") {
        return CallStub<zchat::FriendService::Stub, zchat::GetFriendListReq,
                        zchat::GetFriendListRsp>(
            friend_.get(), &zchat::FriendService::Stub::GetFriendList, body,
            std::move(callback));
    }
    if (path == "/service/friend/get_chat_session_list") {
        return CallStub<zchat::FriendService::Stub,
                        zchat::GetChatSessionListReq,
                        zchat::GetChatSessionListRsp>(
            friend_.get(), &zchat::FriendService::Stub::GetChatSessionList,
            body, std::move(callback));
    }
    if (path == "/service/friend/get_pending_friend_events") {
        return CallStub<zchat::FriendService::Stub,
                        zchat::GetPendingFriendEventListReq,
                        zchat::GetPendingFriendEventListRsp>(
            friend_.get(),
            &zchat::FriendService::Stub::GetPendingFriendEventList, body,
            std::move(callback));
    }
    if (path == "/service/friend/remove_friend") {
        return CallStub<zchat::FriendService::Stub, zchat::FriendRemoveReq,
                        zchat::FriendRemoveRsp>(
            friend_.get(), &zchat::FriendService::Stub::FriendRemove, body,
            std::move(callback));
    }
    if (path == "/service/friend/add_friend_apply") {
        return CallStub<zchat::FriendService::Stub, zchat::FriendAddReq,
                        zchat::FriendAddRsp>(
            friend_.get(), &zchat::FriendService::Stub::FriendAdd, body,
            std::move(callback));
    }
    if (path == "/service/friend/add_friend_process") {
        return CallStub<zchat::FriendService::Stub, zchat::FriendAddProcessReq,
                        zchat::FriendAddProcessRsp>(
            friend_.get(), &zchat::FriendService::Stub::FriendAddProcess, body,
            std::move(callback));
    }
    if (path == "/service/friend/create_chat_session") {
        return CallStub<zchat::FriendService::Stub, zchat::ChatSessionCreateReq,
                        zchat::ChatSessionCreateRsp>(
            friend_.get(), &zchat::FriendService::Stub::ChatSessionCreate, body,
            std::move(callback));
    }
    if (path == "/service/friend/get_chat_session_member") {
        return CallStub<zchat::FriendService::Stub,
                        zchat::GetChatSessionMemberReq,
                        zchat::GetChatSessionMemberRsp>(
            friend_.get(), &zchat::FriendService::Stub::GetChatSessionMember,
            body, std::move(callback));
    }
    if (path == "/service/friend/search_friend") {
        return CallStub<zchat::FriendService::Stub, zchat::FriendSearchReq,
                        zchat::FriendSearchRsp>(
            friend_.get(), &zchat::FriendService::Stub::FriendSearch, body,
            std::move(callback));
    }
    if (path == "/service/message_storage/get_recent") {
        return CallStub<zchat::MsgStorageService::Stub, zchat::GetRecentMsgReq,
                        zchat::GetRecentMsgRsp>(
            message_.get(), &zchat::MsgStorageService::Stub::GetRecentMsg, body,
            std::move(callback));
    }
    if (path == "/service/message_storage/get_history") {
        return CallStub<zchat::MsgStorageService::Stub, zchat::GetHistoryMsgReq,
                        zchat::GetHistoryMsgRsp>(
            message_.get(), &zchat::MsgStorageService::Stub::GetHistoryMsg,
            body, std::move(callback));
    }
    if (path == "/service/message_storage/search_history") {
        return CallStub<zchat::MsgStorageService::Stub, zchat::MsgSearchReq,
                        zchat::MsgSearchRsp>(
            message_.get(), &zchat::MsgStorageService::Stub::MsgSearch, body,
            std::move(callback));
    }
    if (path == "/service/message_transmit/new_message") {
        return CallTransmite(transmite_.get(), body, std::move(callback));
    }
    if (path == "/service/file/get_single_file") {
        return CallStub<zchat::FileService::Stub, zchat::GetSingleFileReq,
                        zchat::GetSingleFileRsp>(
            file_.get(), &zchat::FileService::Stub::GetSingleFile, body,
            std::move(callback));
    }
    if (path == "/service/file/get_multi_file") {
        return CallStub<zchat::FileService::Stub, zchat::GetMultiFileReq,
                        zchat::GetMultiFileRsp>(
            file_.get(), &zchat::FileService::Stub::GetMultiFile, body,
            std::move(callback));
    }
    if (path == "/service/file/put_single_file") {
        return CallStub<zchat::FileService::Stub, zchat::PutSingleFileReq,
                        zchat::PutSingleFileRsp>(
            file_.get(), &zchat::FileService::Stub::PutSingleFile, body,
            std::move(callback));
    }
    if (path == "/service/file/put_multi_file") {
        return CallStub<zchat::FileService::Stub, zchat::PutMultiFileReq,
                        zchat::PutMultiFileRsp>(
            file_.get(), &zchat::FileService::Stub::PutMultiFile, body,
            std::move(callback));
    }
    if (path == "/service/speech/recognition") {
        return CallStub<zchat::SpeechService::Stub, zchat::SpeechRecognitionReq,
                        zchat::SpeechRecognitionRsp>(
            speech_.get(), &zchat::SpeechService::Stub::SpeechRecognition, body,
            std::move(callback));
    }
    ZCHAT_LOG_WARN("unknown service path: {}", path);
    callback(GrpcErrorResponse<zchat::NewMessageRsp>("未知内部服务路径"));
}

} // namespace zchat
