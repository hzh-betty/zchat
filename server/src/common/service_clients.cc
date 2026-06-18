#include "common/service_clients.h"

#include "common/logger.h"

namespace zchat {

ServiceClients::ServiceClients(const EtcdConfig &config) : discovery_(config) {}

drogon::Task<Result<zchat::GetUserInfoRsp>>
ServiceClients::GetUserCoro(const zchat::GetUserInfoReq &request) {
    co_return co_await CallUnaryCoro<zchat::UserService, zchat::GetUserInfoReq,
                                     zchat::GetUserInfoRsp>(
        discovery_, channel_pool_, "user_service",
        [](zchat::UserService::Stub *stub, grpc::ClientContext *ctx,
           const zchat::GetUserInfoReq *req, zchat::GetUserInfoRsp *rsp,
           std::function<void(grpc::Status)> cb) {
            stub->async()->GetUserInfo(ctx, req, rsp, std::move(cb));
        },
        request, kGrpcDeadline);
}

drogon::Task<Result<zchat::GetMultiUserInfoRsp>>
ServiceClients::GetMultiUserInfoCoro(
    const zchat::GetMultiUserInfoReq &request) {
    co_return co_await CallUnaryCoro<zchat::UserService,
                                     zchat::GetMultiUserInfoReq,
                                     zchat::GetMultiUserInfoRsp>(
        discovery_, channel_pool_, "user_service",
        [](zchat::UserService::Stub *stub, grpc::ClientContext *ctx,
           const zchat::GetMultiUserInfoReq *req,
           zchat::GetMultiUserInfoRsp *rsp,
           std::function<void(grpc::Status)> cb) {
            stub->async()->GetMultiUserInfo(ctx, req, rsp, std::move(cb));
        },
        request, kGrpcDeadline);
}

drogon::Task<Result<zchat::SearchUsersRsp>>
ServiceClients::SearchUsersCoro(const zchat::SearchUsersReq &request) {
    co_return co_await CallUnaryCoro<zchat::UserService, zchat::SearchUsersReq,
                                     zchat::SearchUsersRsp>(
        discovery_, channel_pool_, "user_service",
        [](zchat::UserService::Stub *stub, grpc::ClientContext *ctx,
           const zchat::SearchUsersReq *req, zchat::SearchUsersRsp *rsp,
           std::function<void(grpc::Status)> cb) {
            stub->async()->SearchUsers(ctx, req, rsp, std::move(cb));
        },
        request, kGrpcDeadline);
}

drogon::Task<Result<zchat::GetChatSessionMemberIdsRsp>>
ServiceClients::GetChatSessionMemberIdsCoro(
    const zchat::GetChatSessionMemberIdsReq &request) {
    co_return co_await CallUnaryCoro<zchat::FriendService,
                                     zchat::GetChatSessionMemberIdsReq,
                                     zchat::GetChatSessionMemberIdsRsp>(
        discovery_, channel_pool_, "friend_service",
        [](zchat::FriendService::Stub *stub, grpc::ClientContext *ctx,
           const zchat::GetChatSessionMemberIdsReq *req,
           zchat::GetChatSessionMemberIdsRsp *rsp,
           std::function<void(grpc::Status)> cb) {
            stub->async()->GetChatSessionMemberIds(ctx, req, rsp,
                                                   std::move(cb));
        },
        request, kGrpcDeadline);
}

drogon::Task<Result<zchat::GetRecentMsgRsp>>
ServiceClients::GetRecentMsgCoro(const zchat::GetRecentMsgReq &request) {
    co_return co_await CallUnaryCoro<zchat::MsgStorageService,
                                     zchat::GetRecentMsgReq,
                                     zchat::GetRecentMsgRsp>(
        discovery_, channel_pool_, "message_service",
        [](zchat::MsgStorageService::Stub *stub, grpc::ClientContext *ctx,
           const zchat::GetRecentMsgReq *req, zchat::GetRecentMsgRsp *rsp,
           std::function<void(grpc::Status)> cb) {
            stub->async()->GetRecentMsg(ctx, req, rsp, std::move(cb));
        },
        request, kGrpcDeadline);
}

drogon::Task<Result<zchat::GetMultiRecentMsgRsp>>
ServiceClients::GetMultiRecentMsgCoro(
    const zchat::GetMultiRecentMsgReq &request) {
    co_return co_await CallUnaryCoro<zchat::MsgStorageService,
                                     zchat::GetMultiRecentMsgReq,
                                     zchat::GetMultiRecentMsgRsp>(
        discovery_, channel_pool_, "message_service",
        [](zchat::MsgStorageService::Stub *stub, grpc::ClientContext *ctx,
           const zchat::GetMultiRecentMsgReq *req,
           zchat::GetMultiRecentMsgRsp *rsp,
           std::function<void(grpc::Status)> cb) {
            stub->async()->GetMultiRecentMsg(ctx, req, rsp, std::move(cb));
        },
        request, kGrpcDeadline);
}

drogon::Task<Result<std::optional<FileRecord>>>
ServiceClients::GetFileCoro(const std::string &file_id) {
    zchat::GetSingleFileReq request;
    request.set_file_id(file_id);
    auto rsp =
        co_await CallUnaryCoro<zchat::FileService, zchat::GetSingleFileReq,
                               zchat::GetSingleFileRsp>(
            discovery_, channel_pool_, "file_service",
            [](zchat::FileService::Stub *stub, grpc::ClientContext *ctx,
               const zchat::GetSingleFileReq *req, zchat::GetSingleFileRsp *rsp,
               std::function<void(grpc::Status)> cb) {
                stub->async()->GetSingleFile(ctx, req, rsp, std::move(cb));
            },
            request, kFileGrpcDeadline);
    if (!rsp.ok()) {
        co_return Result<std::optional<FileRecord>>::Fail(rsp.error());
    }
    if (!rsp.value().success()) {
        co_return Result<std::optional<FileRecord>>::Ok(std::nullopt);
    }
    FileRecord record;
    record.file_id = rsp.value().file_data().file_id();
    record.file_size = static_cast<std::uint64_t>(
        rsp.value().file_data().file_content().size());
    record.file_content = rsp.value().file_data().file_content();
    co_return Result<std::optional<FileRecord>>::Ok(std::move(record));
}

drogon::Task<Result<zchat::GetMultiFileRsp>>
ServiceClients::GetMultiFileCoro(const std::vector<std::string> &file_ids) {
    zchat::GetMultiFileReq request;
    for (const auto &file_id : file_ids) {
        request.add_file_id_list(file_id);
    }
    co_return co_await CallUnaryCoro<zchat::FileService, zchat::GetMultiFileReq,
                                     zchat::GetMultiFileRsp>(
        discovery_, channel_pool_, "file_service",
        [](zchat::FileService::Stub *stub, grpc::ClientContext *ctx,
           const zchat::GetMultiFileReq *req, zchat::GetMultiFileRsp *rsp,
           std::function<void(grpc::Status)> cb) {
            stub->async()->GetMultiFile(ctx, req, rsp, std::move(cb));
        },
        request, kFileGrpcDeadline);
}

drogon::Task<Result<std::string>>
ServiceClients::PutFileCoro(const std::string &file_name,
                            const std::string &file_content) {
    zchat::PutSingleFileReq request;
    request.mutable_file_data()->set_file_name(file_name);
    request.mutable_file_data()->set_file_size(
        static_cast<std::int64_t>(file_content.size()));
    request.mutable_file_data()->set_file_content(file_content);
    auto rsp =
        co_await CallUnaryCoro<zchat::FileService, zchat::PutSingleFileReq,
                               zchat::PutSingleFileRsp>(
            discovery_, channel_pool_, "file_service",
            [](zchat::FileService::Stub *stub, grpc::ClientContext *ctx,
               const zchat::PutSingleFileReq *req, zchat::PutSingleFileRsp *rsp,
               std::function<void(grpc::Status)> cb) {
                stub->async()->PutSingleFile(ctx, req, rsp, std::move(cb));
            },
            request, kFileGrpcDeadline);
    if (!rsp.ok()) {
        co_return Result<std::string>::Fail(rsp.error());
    }
    if (!rsp.value().success()) {
        co_return Result<std::string>::Fail(
            AppError::WithCode(ErrorCode::kExternalServiceError,
                               "file_service put file failed")
                .WithDetail(rsp.value().errmsg()));
    }
    co_return Result<std::string>::Ok(rsp.value().file_info().file_id());
}

} // namespace zchat
