#ifndef ZCHAT_SERVER_SRC_FILE_FILE_GRPC_SERVICE_H_
#define ZCHAT_SERVER_SRC_FILE_FILE_GRPC_SERVICE_H_

#include "common/noncopyable.h"

#include <memory>

#include <grpcpp/grpcpp.h>

#include "file.grpc.pb.h"
#include "file/file_service.h"

namespace zchat {

class FileGrpcService final : public zchat::FileService::Service,
                              public NonCopyable {
  public:
    explicit FileGrpcService(std::shared_ptr<FileApplicationService> service);

    ~FileGrpcService() override = default;

    grpc::Status GetSingleFile(grpc::ServerContext *context,
                               const zchat::GetSingleFileReq *request,
                               zchat::GetSingleFileRsp *response) override;
    grpc::Status GetMultiFile(grpc::ServerContext *context,
                              const zchat::GetMultiFileReq *request,
                              zchat::GetMultiFileRsp *response) override;
    grpc::Status PutSingleFile(grpc::ServerContext *context,
                               const zchat::PutSingleFileReq *request,
                               zchat::PutSingleFileRsp *response) override;
    grpc::Status PutMultiFile(grpc::ServerContext *context,
                              const zchat::PutMultiFileReq *request,
                              zchat::PutMultiFileRsp *response) override;
    grpc::Status
    GetSingleFileStream(grpc::ServerContext *context,
                        const zchat::GetSingleFileReq *request,
                        grpc::ServerWriter<zchat::FileChunk> *writer) override;
    grpc::Status
    PutSingleFileStream(grpc::ServerContext *context,
                        grpc::ServerReader<zchat::FileChunk> *reader,
                        zchat::PutSingleFileRsp *response) override;

  private:
    std::shared_ptr<FileApplicationService> service_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_FILE_FILE_GRPC_SERVICE_H_
