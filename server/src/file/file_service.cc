#include "file/file_service.h"

#include "common/error_response.h"
#include "common/logger.h"
#include "common/uuid.h"
#include "file/file_errors.h"

namespace zchat {
namespace {

zchat::GetSingleFileRsp ErrorResponse(const std::string &request_id,
                                      const AppError &error) {
    return MakeErrorResponse<zchat::GetSingleFileRsp>(request_id, error);
}

zchat::PutSingleFileRsp PutErrorResponse(const std::string &request_id,
                                         const AppError &error) {
    return MakeErrorResponse<zchat::PutSingleFileRsp>(request_id, error);
}

AppError AccessDenied() {
    return AppError::WithCode(ErrorCode::kForbidden,
                              "access denied to this file");
}

} // namespace

FileApplicationService::FileApplicationService(FileRepository &repository,
                                               ServiceClients &clients,
                                               SessionStore &sessions)
    : repository_(repository), clients_(clients), sessions_(sessions) {}

drogon::Task<bool> FileApplicationService::CheckSessionMemberCachedCoro(
    const std::string &session_id, const std::string &user_id) {

    const std::string cache_key =
        "zchat:sessionmember:" + session_id + ":" + user_id;
    try {
        auto cached = co_await sessions_.GetUserIdCoro(cache_key);
        if (cached.ok() && cached.value().has_value()) {
            co_return cached.value().value() == "1";
        }
    } catch (...) {
    }

    zchat::GetChatSessionMemberIdsReq req;
    req.set_request_id("internal-idor");
    req.set_chat_session_id(session_id);
    auto rsp = co_await clients_.GetChatSessionMemberIdsCoro(req);
    if (!rsp.ok() || !rsp.value().success()) {
        co_return false;
    }
    bool is_member = false;
    for (const auto &mid : rsp.value().member_id()) {
        if (mid == user_id) {
            is_member = true;
            break;
        }
    }

    try {
        co_await sessions_.SaveVerifyCodeCoro(cache_key, is_member ? "1" : "0");
    } catch (...) {
    }

    co_return is_member;
}

drogon::Task<bool>
FileApplicationService::CanAccessFileCoro(const FileRecord &file,
                                          const std::string &caller_user_id) {
    if (caller_user_id.empty()) {
        co_return false;
    }
    if (file.owner_user_id == caller_user_id) {
        co_return true;
    }
    if (!file.chat_session_id.empty()) {
        co_return co_await CheckSessionMemberCachedCoro(file.chat_session_id,
                                                        caller_user_id);
    }
    co_return false;
}

drogon::Task<zchat::GetSingleFileRsp>
FileApplicationService::GetSingleFileInternal(const std::string &request_id,
                                              const std::string &file_id,
                                              const std::string &caller) {
    auto file = co_await repository_.GetFileCoro(file_id);
    if (!file.ok()) {
        co_return ErrorResponse(request_id, file.error());
    }
    if (!file.value().has_value()) {
        co_return ErrorResponse(request_id, file_errors::FileNotFound());
    }
    const auto &record = file.value().value();
    auto can_access = co_await CanAccessFileCoro(record, caller);
    if (!can_access) {
        co_return ErrorResponse(request_id, AccessDenied());
    }
    zchat::GetSingleFileRsp response;
    response.set_request_id(request_id);
    response.set_success(true);
    response.set_errmsg("");
    response.mutable_file_data()->set_file_id(record.file_id);
    response.mutable_file_data()->set_file_content(record.file_content);
    co_return response;
}

drogon::Task<zchat::GetSingleFileRsp> FileApplicationService::GetSingleFileCoro(
    const zchat::GetSingleFileReq &request) {
    ZCHAT_LOG_INFO("FileService::GetSingleFile request_id={}",
                   request.request_id());
    std::string caller = request.has_user_id() ? request.user_id() : "";
    co_return co_await GetSingleFileInternal(request.request_id(),
                                             request.file_id(), caller);
}

drogon::Task<zchat::GetMultiFileRsp> FileApplicationService::GetMultiFileCoro(
    const zchat::GetMultiFileReq &request) {
    ZCHAT_LOG_INFO("FileService::GetMultiFile request_id={}",
                   request.request_id());
    std::string caller = request.has_user_id() ? request.user_id() : "";
    zchat::GetMultiFileRsp response;
    response.set_request_id(request.request_id());
    response.set_success(true);
    response.set_errmsg("");
    for (const auto &file_id : request.file_id_list()) {
        auto single = co_await GetSingleFileInternal(request.request_id(),
                                                     file_id, caller);
        if (single.success()) {
            *response.add_file_data() = single.file_data();
        }
    }
    co_return response;
}

drogon::Task<zchat::PutSingleFileRsp> FileApplicationService::PutSingleFileCoro(
    const zchat::PutSingleFileReq &request) {
    ZCHAT_LOG_INFO("FileService::PutSingleFile request_id={}",
                   request.request_id());
    const std::string file_id = NewId();
    const auto &upload = request.file_data();
    FileRecord record;
    record.file_id = file_id;
    record.file_name = upload.file_name();
    record.file_size = static_cast<std::uint64_t>(upload.file_size());
    record.file_content = upload.file_content();
    record.owner_user_id = request.has_user_id() ? request.user_id() : "";
    record.chat_session_id =
        request.has_session_id() ? request.session_id() : "";
    const auto stored = co_await repository_.PutFileCoro(record);
    if (!stored.ok()) {
        co_return PutErrorResponse(request.request_id(), stored.error());
    }
    zchat::PutSingleFileRsp response;
    response.set_request_id(request.request_id());
    response.set_success(true);
    response.set_errmsg("");
    response.mutable_file_info()->set_file_id(file_id);
    response.mutable_file_info()->set_file_name(upload.file_name());
    response.mutable_file_info()->set_file_size(upload.file_size());
    co_return response;
}

drogon::Task<zchat::PutMultiFileRsp> FileApplicationService::PutMultiFileCoro(
    const zchat::PutMultiFileReq &request) {
    ZCHAT_LOG_INFO("FileService::PutMultiFile request_id={}",
                   request.request_id());
    std::string owner = request.has_user_id() ? request.user_id() : "";
    std::string session = request.has_session_id() ? request.session_id() : "";
    zchat::PutMultiFileRsp response;
    response.set_request_id(request.request_id());
    response.set_success(true);
    response.set_errmsg("");
    for (const auto &upload : request.file_data()) {
        const std::string file_id = NewId();
        FileRecord record;
        record.file_id = file_id;
        record.file_name = upload.file_name();
        record.file_size = static_cast<std::uint64_t>(upload.file_size());
        record.file_content = upload.file_content();
        record.owner_user_id = owner;
        record.chat_session_id = session;
        const auto stored = co_await repository_.PutFileCoro(record);
        if (!stored.ok()) {
            response.set_success(false);
            response.set_errmsg(FormatErrorForClient(stored.error()));
            co_return response;
        }
        auto *info = response.add_file_info();
        info->set_file_id(file_id);
        info->set_file_name(upload.file_name());
        info->set_file_size(upload.file_size());
    }
    co_return response;
}

} // namespace zchat
