#include "file/file_service.h"

#include "common/uuid.h"

namespace zchat {
namespace {

zchat::GetSingleFileRsp ErrorResponse(const std::string &request_id,
                                      const std::string &message) {
    zchat::GetSingleFileRsp response;
    response.set_request_id(request_id);
    response.set_success(false);
    response.set_errmsg(message);
    return response;
}

zchat::PutSingleFileRsp PutErrorResponse(const std::string &request_id,
                                         const std::string &message) {
    zchat::PutSingleFileRsp response;
    response.set_request_id(request_id);
    response.set_success(false);
    response.set_errmsg(message);
    return response;
}

} // namespace

FileApplicationService::FileApplicationService(FileRepository &repository)
    : repository_(repository) {}

zchat::GetSingleFileRsp
FileApplicationService::GetSingleFile(const zchat::GetSingleFileReq &request) {
    auto file = repository_.GetFile(request.file_id());
    if (!file.ok()) {
        return ErrorResponse(request.request_id(), file.error().message);
    }
    if (!file.value().has_value()) {
        return ErrorResponse(request.request_id(), "文件不存在");
    }
    zchat::GetSingleFileRsp response;
    response.set_request_id(request.request_id());
    response.set_success(true);
    response.set_errmsg("");
    response.mutable_file_data()->set_file_id(file.value()->file_id);
    response.mutable_file_data()->set_file_content(file.value()->file_content);
    return response;
}

zchat::GetMultiFileRsp
FileApplicationService::GetMultiFile(const zchat::GetMultiFileReq &request) {
    zchat::GetMultiFileRsp response;
    response.set_request_id(request.request_id());
    response.set_success(true);
    response.set_errmsg("");
    for (const auto &file_id : request.file_id_list()) {
        auto file = repository_.GetFile(file_id);
        if (!file.ok()) {
            response.set_success(false);
            response.set_errmsg(file.error().message);
            return response;
        }
        if (file.value().has_value()) {
            zchat::FileDownloadData data;
            data.set_file_id(file.value()->file_id);
            data.set_file_content(file.value()->file_content);
            (*response.mutable_file_data())[file_id] = data;
        }
    }
    return response;
}

zchat::PutSingleFileRsp
FileApplicationService::PutSingleFile(const zchat::PutSingleFileReq &request) {
    const std::string file_id = NewId();
    const auto &upload = request.file_data();
    const auto stored = repository_.PutFile(FileRecord{
        file_id,
        upload.file_name(),
        static_cast<std::uint64_t>(upload.file_size()),
        upload.file_content(),
    });
    if (!stored.ok()) {
        return PutErrorResponse(request.request_id(), stored.error().message);
    }
    zchat::PutSingleFileRsp response;
    response.set_request_id(request.request_id());
    response.set_success(true);
    response.set_errmsg("");
    response.mutable_file_info()->set_file_id(file_id);
    response.mutable_file_info()->set_file_name(upload.file_name());
    response.mutable_file_info()->set_file_size(upload.file_size());
    return response;
}

zchat::PutMultiFileRsp
FileApplicationService::PutMultiFile(const zchat::PutMultiFileReq &request) {
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
            response.set_errmsg(stored.error().message);
            return response;
        }
        auto *info = response.add_file_info();
        info->set_file_id(file_id);
        info->set_file_name(upload.file_name());
        info->set_file_size(upload.file_size());
    }
    return response;
}

} // namespace zchat
