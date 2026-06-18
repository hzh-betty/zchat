#ifndef ZCHAT_SERVER_SRC_FILE_FILE_GRPC_SERVICE_H_
#define ZCHAT_SERVER_SRC_FILE_FILE_GRPC_SERVICE_H_

#include "common/noncopyable.h"

#include <memory>

#include <grpcpp/grpcpp.h>

#include "file.grpc.pb.h"
#include "file/file_service.h"

namespace zchat {

class FileGrpcService final : public zchat::FileService::CallbackService,
                              public NonCopyable {
  public:
    explicit FileGrpcService(std::shared_ptr<FileApplicationService> service);
    ~FileGrpcService() override = default;

    grpc::ServerUnaryReactor *
    GetSingleFile(grpc::CallbackServerContext *context,
                  const zchat::GetSingleFileReq *request,
                  zchat::GetSingleFileRsp *response) override;
    grpc::ServerUnaryReactor *
    GetMultiFile(grpc::CallbackServerContext *context,
                 const zchat::GetMultiFileReq *request,
                 zchat::GetMultiFileRsp *response) override;
    grpc::ServerUnaryReactor *
    PutSingleFile(grpc::CallbackServerContext *context,
                  const zchat::PutSingleFileReq *request,
                  zchat::PutSingleFileRsp *response) override;
    grpc::ServerUnaryReactor *
    PutMultiFile(grpc::CallbackServerContext *context,
                 const zchat::PutMultiFileReq *request,
                 zchat::PutMultiFileRsp *response) override;
    grpc::ServerWriteReactor<zchat::FileChunk> *
    GetSingleFileStream(grpc::CallbackServerContext *context,
                        const zchat::GetSingleFileReq *request) override;
    grpc::ServerReadReactor<zchat::FileChunk> *
    PutSingleFileStream(grpc::CallbackServerContext *context,
                        zchat::PutSingleFileRsp *response) override;

  private:
    std::shared_ptr<FileApplicationService> service_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_FILE_FILE_GRPC_SERVICE_H_
