#include "file/file_grpc_service.h"

#include <utility>

#include "common/error_response.h"

namespace zchat {

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

} // namespace zchat
