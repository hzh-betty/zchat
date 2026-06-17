#include "file/file_grpc_service.h"

#include <algorithm>
#include <utility>

#include "common/error_response.h"
#include "common/logger.h"

namespace zchat {

namespace {

constexpr std::size_t kFileChunkSize = 256 * 1024;

} // namespace

FileGrpcService::FileGrpcService(
    std::shared_ptr<FileApplicationService> service)
    : service_(std::move(service)) {}

grpc::Status
FileGrpcService::GetSingleFile(grpc::ServerContext *,
                               const zchat::GetSingleFileReq *request,
                               zchat::GetSingleFileRsp *response) {
    *response = service_->GetSingleFile(*request);
    LogBoundaryResponseError("FileService", "GetSingleFile",
                             request->request_id(), *response);
    return grpc::Status::OK;
}

grpc::Status
FileGrpcService::GetMultiFile(grpc::ServerContext *,
                              const zchat::GetMultiFileReq *request,
                              zchat::GetMultiFileRsp *response) {
    *response = service_->GetMultiFile(*request);
    LogBoundaryResponseError("FileService", "GetMultiFile",
                             request->request_id(), *response);
    return grpc::Status::OK;
}

grpc::Status
FileGrpcService::PutSingleFile(grpc::ServerContext *,
                               const zchat::PutSingleFileReq *request,
                               zchat::PutSingleFileRsp *response) {
    *response = service_->PutSingleFile(*request);
    LogBoundaryResponseError("FileService", "PutSingleFile",
                             request->request_id(), *response);
    return grpc::Status::OK;
}

grpc::Status
FileGrpcService::PutMultiFile(grpc::ServerContext *,
                              const zchat::PutMultiFileReq *request,
                              zchat::PutMultiFileRsp *response) {
    *response = service_->PutMultiFile(*request);
    LogBoundaryResponseError("FileService", "PutMultiFile",
                             request->request_id(), *response);
    return grpc::Status::OK;
}

grpc::Status FileGrpcService::GetSingleFileStream(
    grpc::ServerContext *, const zchat::GetSingleFileReq *request,
    grpc::ServerWriter<zchat::FileChunk> *writer) {
    ZCHAT_LOG_INFO("FileService::GetSingleFileStream request_id={}",
                   request->request_id());
    auto file = service_->GetFileForDownload(request->file_id());
    if (!file.ok()) {
        return grpc::Status(grpc::StatusCode::INTERNAL, file.error().message);
    }
    if (!file.value().has_value()) {
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "file not found");
    }
    const auto &record = file.value().value();
    const std::string &content = record.file_content;
    const std::size_t total = content.size();
    std::size_t offset = 0;
    while (offset < total) {
        const std::size_t end = std::min(offset + kFileChunkSize, total);
        zchat::FileChunk chunk;
        chunk.set_file_id(record.file_id);
        if (offset == 0) {
            chunk.set_file_name(record.file_name);
            chunk.set_file_size(static_cast<std::int64_t>(record.file_size));
        }
        chunk.set_offset(static_cast<std::int64_t>(offset));
        chunk.set_chunk_data(content.substr(offset, end - offset));
        chunk.set_last(end == total);
        if (!writer->Write(chunk)) {
            return grpc::Status(grpc::StatusCode::CANCELLED,
                                "client cancelled stream");
        }
        offset = end;
    }
    if (total == 0) {
        zchat::FileChunk chunk;
        chunk.set_file_id(record.file_id);
        chunk.set_file_name(record.file_name);
        chunk.set_file_size(0);
        chunk.set_offset(0);
        chunk.set_last(true);
        writer->Write(chunk);
    }
    ZCHAT_LOG_INFO("FileService::GetSingleFileStream success: request_id={}",
                   request->request_id());
    return grpc::Status::OK;
}

grpc::Status FileGrpcService::PutSingleFileStream(
    grpc::ServerContext *, grpc::ServerReader<zchat::FileChunk> *reader,
    zchat::PutSingleFileRsp *response) {
    ZCHAT_LOG_INFO("FileService::PutSingleFileStream");
    zchat::FileChunk chunk;
    std::string file_name;
    std::int64_t file_size = 0;
    std::string content;
    bool first = true;
    while (reader->Read(&chunk)) {
        if (first) {
            file_name = chunk.file_name();
            file_size = chunk.file_size();
            first = false;
        }
        content.append(chunk.chunk_data());
    }
    auto file_id = service_->StoreFileContent(
        file_name, static_cast<std::uint64_t>(file_size), content);
    if (!file_id.ok()) {
        response->set_success(false);
        response->set_errmsg(FormatErrorForClient(file_id.error()));
        return grpc::Status::OK;
    }
    response->set_success(true);
    response->set_errmsg("");
    response->mutable_file_info()->set_file_id(file_id.value());
    response->mutable_file_info()->set_file_name(file_name);
    response->mutable_file_info()->set_file_size(file_size);
    ZCHAT_LOG_INFO("FileService::PutSingleFileStream success: file_id={}",
                   file_id.value());
    return grpc::Status::OK;
}

} // namespace zchat
