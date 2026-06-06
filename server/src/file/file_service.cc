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

} // namespace

FileApplicationService::FileApplicationService(FileRepository &repository)
    : repository_(repository) {}

zchat::GetSingleFileRsp
FileApplicationService::GetSingleFile(const zchat::GetSingleFileReq &request) {
    ZCHAT_LOG_INFO("FileService::GetSingleFile request_id={}",
                   request.request_id());
    auto file = repository_.GetFile(request.file_id());
    if (!file.ok()) {
        return ErrorResponse(request.request_id(), file.error());
    }
    if (!file.value().has_value()) {
        return ErrorResponse(request.request_id(), file_errors::FileNotFound());
    }
    zchat::GetSingleFileRsp response;
    response.set_request_id(request.request_id());
    response.set_success(true);
    response.set_errmsg("");
    response.mutable_file_data()->set_file_id(file.value()->file_id);
    response.mutable_file_data()->set_file_content(file.value()->file_content);
    ZCHAT_LOG_INFO("FileService::GetSingleFile success: request_id={}",
                   request.request_id());
    return response;
}

zchat::GetMultiFileRsp
FileApplicationService::GetMultiFile(const zchat::GetMultiFileReq &request) {
    ZCHAT_LOG_INFO("FileService::GetMultiFile request_id={}",
                   request.request_id());
    zchat::GetMultiFileRsp response;
    response.set_request_id(request.request_id());
    response.set_success(true);
    response.set_errmsg("");
    for (const auto &file_id : request.file_id_list()) {
        auto file = repository_.GetFile(file_id);
        if (!file.ok()) {
            response.set_success(false);
            response.set_errmsg(FormatErrorForClient(file.error()));
            return response;
        }
        if (file.value().has_value()) {
            auto *data = response.add_file_data();
            data->set_file_id(file.value()->file_id);
            data->set_file_content(file.value()->file_content);
        }
    }
    ZCHAT_LOG_INFO("FileService::GetMultiFile success: request_id={}",
                   request.request_id());
    return response;
}

zchat::PutSingleFileRsp
FileApplicationService::PutSingleFile(const zchat::PutSingleFileReq &request) {
    ZCHAT_LOG_INFO("FileService::PutSingleFile request_id={}",
                   request.request_id());
    const std::string file_id = NewId();
    const auto &upload = request.file_data();
    const auto stored = repository_.PutFile(FileRecord{
        file_id,
        upload.file_name(),
        static_cast<std::uint64_t>(upload.file_size()),
        upload.file_content(),
    });
    if (!stored.ok()) {
        return PutErrorResponse(request.request_id(), stored.error());
    }
    zchat::PutSingleFileRsp response;
    response.set_request_id(request.request_id());
    response.set_success(true);
    response.set_errmsg("");
    response.mutable_file_info()->set_file_id(file_id);
    response.mutable_file_info()->set_file_name(upload.file_name());
    response.mutable_file_info()->set_file_size(upload.file_size());
    ZCHAT_LOG_INFO(
        "FileService::PutSingleFile success: request_id={} file_id={}",
        request.request_id(), file_id);
    return response;
}

zchat::PutMultiFileRsp
FileApplicationService::PutMultiFile(const zchat::PutMultiFileReq &request) {
    ZCHAT_LOG_INFO("FileService::PutMultiFile request_id={}",
                   request.request_id());
    zchat::PutMultiFileRsp response;
    response.set_request_id(request.request_id());
    response.set_success(true);
    response.set_errmsg("");
    for (const auto &upload : request.file_data()) {
        const std::string file_id = NewId();
        const auto stored = repository_.PutFile(FileRecord{
            file_id,
            upload.file_name(),
            static_cast<std::uint64_t>(upload.file_size()),
            upload.file_content(),
        });
        if (!stored.ok()) {
            response.set_success(false);
            response.set_errmsg(FormatErrorForClient(stored.error()));
            return response;
        }
        auto *info = response.add_file_info();
        info->set_file_id(file_id);
        info->set_file_name(upload.file_name());
        info->set_file_size(upload.file_size());
    }
    ZCHAT_LOG_INFO("FileService::PutMultiFile success: request_id={}",
                   request.request_id());
    return response;
}

} // namespace zchat
