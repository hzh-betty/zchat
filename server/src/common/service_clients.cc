#include "common/service_clients.h"

#include "common/logger.h"

namespace zchat {

ServiceClients::ServiceClients(const EtcdConfig &config) : discovery_(config) {}

Result<zchat::GetUserInfoRsp>
ServiceClients::GetUser(const zchat::GetUserInfoReq &request) {
    return CallUnary<zchat::UserService>("user_service", kGrpcDeadline,
                                         &zchat::UserService::Stub::GetUserInfo,
                                         request);
}

Result<zchat::GetMultiUserInfoRsp>
ServiceClients::GetMultiUserInfo(const zchat::GetMultiUserInfoReq &request) {
    return CallUnary<zchat::UserService>(
        "user_service", kGrpcDeadline,
        &zchat::UserService::Stub::GetMultiUserInfo, request);
}

Result<zchat::SearchUsersRsp>
ServiceClients::SearchUsers(const zchat::SearchUsersReq &request) {
    return CallUnary<zchat::UserService>("user_service", kGrpcDeadline,
                                         &zchat::UserService::Stub::SearchUsers,
                                         request);
}

Result<zchat::GetChatSessionMemberIdsRsp>
ServiceClients::GetChatSessionMemberIds(
    const zchat::GetChatSessionMemberIdsReq &request) {
    return CallUnary<zchat::FriendService>(
        "friend_service", kGrpcDeadline,
        &zchat::FriendService::Stub::GetChatSessionMemberIds, request);
}

Result<zchat::GetRecentMsgRsp>
ServiceClients::GetRecentMsg(const zchat::GetRecentMsgReq &request) {
    return CallUnary<zchat::MsgStorageService>(
        "message_service", kGrpcDeadline,
        &zchat::MsgStorageService::Stub::GetRecentMsg, request);
}

Result<std::optional<FileRecord>>
ServiceClients::GetFile(const std::string &file_id) {
    zchat::GetSingleFileReq request;
    request.set_file_id(file_id);
    auto rsp = CallUnary<zchat::FileService>(
        "file_service", kFileGrpcDeadline,
        &zchat::FileService::Stub::GetSingleFile, request);
    if (!rsp.ok()) {
        return Result<std::optional<FileRecord>>::Fail(rsp.error());
    }
    if (!rsp.value().success()) {
        return Result<std::optional<FileRecord>>::Ok(std::nullopt);
    }
    FileRecord record;
    record.file_id = rsp.value().file_data().file_id();
    record.file_size = static_cast<std::uint64_t>(
        rsp.value().file_data().file_content().size());
    record.file_content = rsp.value().file_data().file_content();
    return Result<std::optional<FileRecord>>::Ok(std::move(record));
}

Result<zchat::GetMultiFileRsp>
ServiceClients::GetMultiFile(const std::vector<std::string> &file_ids) {
    zchat::GetMultiFileReq request;
    for (const auto &file_id : file_ids) {
        request.add_file_id_list(file_id);
    }
    return CallUnary<zchat::FileService>(
        "file_service", kFileGrpcDeadline,
        &zchat::FileService::Stub::GetMultiFile, request);
}

Result<std::string> ServiceClients::PutFile(const std::string &file_name,
                                            const std::string &file_content) {
    zchat::PutSingleFileReq request;
    request.mutable_file_data()->set_file_name(file_name);
    request.mutable_file_data()->set_file_size(
        static_cast<std::int64_t>(file_content.size()));
    request.mutable_file_data()->set_file_content(file_content);
    auto rsp = CallUnary<zchat::FileService>(
        "file_service", kFileGrpcDeadline,
        &zchat::FileService::Stub::PutSingleFile, request);
    if (!rsp.ok()) {
        return Result<std::string>::Fail(rsp.error());
    }
    if (!rsp.value().success()) {
        return Result<std::string>::Fail(
            AppError::WithCode(ErrorCode::kExternalServiceError,
                               "file_service put file failed")
                .WithDetail(rsp.value().errmsg()));
    }
    return Result<std::string>::Ok(rsp.value().file_info().file_id());
}

} // namespace zchat
