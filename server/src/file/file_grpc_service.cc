#include "file/file_grpc_service.h"

#include <utility>

namespace zchat {

FileGrpcService::FileGrpcService(
    std::shared_ptr<FileApplicationService> service)
    : service_(std::move(service)) {}

grpc::Status
FileGrpcService::GetSingleFile(grpc::ServerContext *,
                               const zchat::GetSingleFileReq *request,
                               zchat::GetSingleFileRsp *response) {
    *response = service_->GetSingleFile(*request);
    return grpc::Status::OK;
}

grpc::Status
FileGrpcService::GetMultiFile(grpc::ServerContext *,
                              const zchat::GetMultiFileReq *request,
                              zchat::GetMultiFileRsp *response) {
    *response = service_->GetMultiFile(*request);
    return grpc::Status::OK;
}

grpc::Status
FileGrpcService::PutSingleFile(grpc::ServerContext *,
                               const zchat::PutSingleFileReq *request,
                               zchat::PutSingleFileRsp *response) {
    *response = service_->PutSingleFile(*request);
    return grpc::Status::OK;
}

grpc::Status
FileGrpcService::PutMultiFile(grpc::ServerContext *,
                              const zchat::PutMultiFileReq *request,
                              zchat::PutMultiFileRsp *response) {
    *response = service_->PutMultiFile(*request);
    return grpc::Status::OK;
}

} // namespace zchat
