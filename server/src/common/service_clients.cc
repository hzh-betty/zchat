#include "common/service_clients.h"

#include "common/logger.h"

namespace zchat {

ServiceClients::ServiceClients(const EtcdConfig &config) : discovery_(config) {}

std::shared_ptr<grpc::Channel>
ServiceClients::GetOrCreateChannel(const std::string &endpoint) {
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

Result<zchat::GetUserInfoRsp>
ServiceClients::GetUser(const zchat::GetUserInfoReq &request) {
    auto endpoint = discovery_.Endpoint("user_service");
    if (!endpoint.ok()) {
        return Result<zchat::GetUserInfoRsp>::Fail(endpoint.error());
    }
    auto stub =
        zchat::UserService::NewStub(GetOrCreateChannel(endpoint.value()));
    zchat::GetUserInfoRsp response;
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + kGrpcDeadline);
    const grpc::Status status = stub->GetUserInfo(&context, request, &response);
    if (!status.ok()) {
        return Result<zchat::GetUserInfoRsp>::Fail(
            AppError::WithCode(ErrorCode::kExternalServiceError,
                               "user service grpc call failed")
                .WithDetail(status.error_message()));
    }
    return Result<zchat::GetUserInfoRsp>::Ok(std::move(response));
}

Result<zchat::GetMultiUserInfoRsp>
ServiceClients::GetMultiUserInfo(const zchat::GetMultiUserInfoReq &request) {
    auto endpoint = discovery_.Endpoint("user_service");
    if (!endpoint.ok()) {
        return Result<zchat::GetMultiUserInfoRsp>::Fail(endpoint.error());
    }
    auto stub =
        zchat::UserService::NewStub(GetOrCreateChannel(endpoint.value()));
    zchat::GetMultiUserInfoRsp response;
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + kGrpcDeadline);
    const grpc::Status status =
        stub->GetMultiUserInfo(&context, request, &response);
    if (!status.ok()) {
        return Result<zchat::GetMultiUserInfoRsp>::Fail(
            AppError::WithCode(ErrorCode::kExternalServiceError,
                               "user service grpc call failed")
                .WithDetail(status.error_message()));
    }
    return Result<zchat::GetMultiUserInfoRsp>::Ok(std::move(response));
}

Result<zchat::GetChatSessionMemberIdsRsp>
ServiceClients::GetChatSessionMemberIds(
    const zchat::GetChatSessionMemberIdsReq &request) {
    auto endpoint = discovery_.Endpoint("friend_service");
    if (!endpoint.ok()) {
        return Result<zchat::GetChatSessionMemberIdsRsp>::Fail(
            endpoint.error());
    }
    auto stub =
        zchat::FriendService::NewStub(GetOrCreateChannel(endpoint.value()));
    zchat::GetChatSessionMemberIdsRsp response;
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + kGrpcDeadline);
    const grpc::Status status =
        stub->GetChatSessionMemberIds(&context, request, &response);
    if (!status.ok()) {
        return Result<zchat::GetChatSessionMemberIdsRsp>::Fail(
            AppError::WithCode(ErrorCode::kExternalServiceError,
                               "friend service grpc call failed")
                .WithDetail(status.error_message()));
    }
    return Result<zchat::GetChatSessionMemberIdsRsp>::Ok(std::move(response));
}

Result<zchat::GetRecentMsgRsp>
ServiceClients::GetRecentMsg(const zchat::GetRecentMsgReq &request) {
    auto endpoint = discovery_.Endpoint("message_service");
    if (!endpoint.ok()) {
        return Result<zchat::GetRecentMsgRsp>::Fail(endpoint.error());
    }
    auto stub =
        zchat::MsgStorageService::NewStub(GetOrCreateChannel(endpoint.value()));
    zchat::GetRecentMsgRsp response;
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + kGrpcDeadline);
    const grpc::Status status =
        stub->GetRecentMsg(&context, request, &response);
    if (!status.ok()) {
        return Result<zchat::GetRecentMsgRsp>::Fail(
            AppError::WithCode(ErrorCode::kExternalServiceError,
                               "message service grpc call failed")
                .WithDetail(status.error_message()));
    }
    return Result<zchat::GetRecentMsgRsp>::Ok(std::move(response));
}

Result<std::optional<FileRecord>>
ServiceClients::GetFile(const std::string &file_id) {
    auto endpoint = discovery_.Endpoint("file_service");
    if (!endpoint.ok()) {
        return Result<std::optional<FileRecord>>::Fail(endpoint.error());
    }
    auto stub =
        zchat::FileService::NewStub(GetOrCreateChannel(endpoint.value()));
    zchat::GetSingleFileReq request;
    request.set_file_id(file_id);
    zchat::GetSingleFileRsp response;
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + kFileGrpcDeadline);
    const grpc::Status status =
        stub->GetSingleFile(&context, request, &response);
    if (!status.ok()) {
        return Result<std::optional<FileRecord>>::Fail(
            AppError::WithCode(ErrorCode::kExternalServiceError,
                               "file service grpc call failed")
                .WithDetail(status.error_message()));
    }
    if (!response.success()) {
        return Result<std::optional<FileRecord>>::Ok(std::nullopt);
    }
    FileRecord record;
    record.file_id = response.file_data().file_id();
    record.file_size =
        static_cast<std::uint64_t>(response.file_data().file_content().size());
    record.file_content = response.file_data().file_content();
    return Result<std::optional<FileRecord>>::Ok(std::move(record));
}

Result<std::string> ServiceClients::PutFile(const std::string &file_name,
                                            const std::string &file_content) {
    auto endpoint = discovery_.Endpoint("file_service");
    if (!endpoint.ok()) {
        return Result<std::string>::Fail(endpoint.error());
    }
    auto stub =
        zchat::FileService::NewStub(GetOrCreateChannel(endpoint.value()));
    zchat::PutSingleFileReq request;
    request.mutable_file_data()->set_file_name(file_name);
    request.mutable_file_data()->set_file_size(
        static_cast<std::int64_t>(file_content.size()));
    request.mutable_file_data()->set_file_content(file_content);
    zchat::PutSingleFileRsp response;
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + kFileGrpcDeadline);
    const grpc::Status status =
        stub->PutSingleFile(&context, request, &response);
    if (!status.ok()) {
        return Result<std::string>::Fail(
            AppError::WithCode(ErrorCode::kExternalServiceError,
                               "file service grpc call failed")
                .WithDetail(status.error_message()));
    }
    if (!response.success()) {
        return Result<std::string>::Fail(
            AppError::WithCode(ErrorCode::kExternalServiceError,
                               "file service put file failed")
                .WithDetail(response.errmsg()));
    }
    return Result<std::string>::Ok(response.file_info().file_id());
}

} // namespace zchat