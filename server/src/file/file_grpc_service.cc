#include "file/file_grpc_service.h"

#include <utility>

#include "common/error_response.h"
#include "common/grpc_callback.h"
#include "common/logger.h"

namespace zchat {

FileGrpcService::FileGrpcService(
    std::shared_ptr<FileApplicationService> service)
    : service_(std::move(service)) {}

#define ZCHAT_FILE_RPC(method_name, req_type, rsp_type, coro_name)             \
    grpc::ServerUnaryReactor *FileGrpcService::method_name(                    \
        grpc::CallbackServerContext *, const zchat::req_type *request,         \
        zchat::rsp_type *response) {                                           \
        ZCHAT_LOG_INFO("FileService::" #method_name " request_id={}",          \
                       request->request_id());                                 \
        return new CoroUnaryReactor<zchat::rsp_type>(                          \
            [this, req = *request]() -> drogon::Task<zchat::rsp_type> {        \
                co_return co_await service_->coro_name(req);                   \
            },                                                                 \
            response, "FileService", #method_name, request->request_id());     \
    }

ZCHAT_FILE_RPC(GetSingleFile, GetSingleFileReq, GetSingleFileRsp,
               GetSingleFileCoro)
ZCHAT_FILE_RPC(GetMultiFile, GetMultiFileReq, GetMultiFileRsp, GetMultiFileCoro)
ZCHAT_FILE_RPC(PutSingleFile, PutSingleFileReq, PutSingleFileRsp,
               PutSingleFileCoro)
ZCHAT_FILE_RPC(PutMultiFile, PutMultiFileReq, PutMultiFileRsp, PutMultiFileCoro)

} // namespace zchat
