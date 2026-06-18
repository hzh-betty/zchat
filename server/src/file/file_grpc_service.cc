#include "file/file_grpc_service.h"

#include <algorithm>
#include <atomic>
#include <utility>

#include "common/error_response.h"
#include "common/grpc_callback.h"
#include "common/logger.h"

namespace zchat {

namespace {

constexpr std::size_t kFileChunkSize = 256 * 1024;

class GetFileStreamReactor final
    : public grpc::ServerWriteReactor<zchat::FileChunk> {
  public:
    GetFileStreamReactor(std::shared_ptr<FileApplicationService> service,
                         const zchat::GetSingleFileReq &request)
        : service_(std::move(service)), request_id_(request.request_id()) {
        Launch(request.file_id());
    }

    void OnWriteDone(bool ok) override {
        if (!ok || cancelled_.load()) {
            SafeFinish(
                grpc::Status(grpc::StatusCode::CANCELLED, "write failed"));
            return;
        }
        WriteNextChunk();
    }

    void OnDone() override { delete this; }

    void OnCancel() override {
        cancelled_.store(true);
        SafeFinish(grpc::Status(grpc::StatusCode::CANCELLED, "cancelled"));
    }

  private:
    void SafeFinish(const grpc::Status &status) {
        if (!finished_.exchange(true)) {
            Finish(status);
        }
    }

    void Launch(const std::string &file_id) {
        [](GetFileStreamReactor *self,
           std::string file_id) -> drogon::AsyncTask {
            auto file =
                co_await self->service_->GetFileForDownloadCoro(file_id);
            if (self->cancelled_.load()) {
                co_return;
            }
            if (!file.ok()) {
                self->SafeFinish(grpc::Status(grpc::StatusCode::INTERNAL,
                                              file.error().message));
                co_return;
            }
            if (!file.value().has_value()) {
                self->SafeFinish(grpc::Status(grpc::StatusCode::NOT_FOUND,
                                              "file not found"));
                co_return;
            }
            self->record_ = std::move(file.value().value());
            self->content_ = &self->record_.file_content;
            self->total_ = self->content_->size();
            self->offset_ = 0;
            self->WriteNextChunk();
            co_return;
        }(this, file_id);
    }

    void WriteNextChunk() {
        if (offset_ >= total_) {
            if (total_ == 0) {
                zchat::FileChunk chunk;
                chunk.set_file_id(record_.file_id);
                chunk.set_file_name(record_.file_name);
                chunk.set_file_size(0);
                chunk.set_offset(0);
                chunk.set_last(true);
                StartWrite(&chunk);
                return;
            }
            SafeFinish(grpc::Status::OK);
            return;
        }
        const std::size_t end = std::min(offset_ + kFileChunkSize, total_);
        chunk_.Clear();
        chunk_.set_file_id(record_.file_id);
        if (offset_ == 0) {
            chunk_.set_file_name(record_.file_name);
            chunk_.set_file_size(static_cast<std::int64_t>(record_.file_size));
        }
        chunk_.set_offset(static_cast<std::int64_t>(offset_));
        chunk_.set_chunk_data(content_->substr(offset_, end - offset_));
        chunk_.set_last(end == total_);
        offset_ = end;
        StartWrite(&chunk_);
    }

    std::shared_ptr<FileApplicationService> service_;
    std::string request_id_;
    FileRecord record_;
    const std::string *content_ = nullptr;
    std::size_t total_ = 0;
    std::size_t offset_ = 0;
    zchat::FileChunk chunk_;
    std::atomic_bool finished_{false};
    std::atomic_bool cancelled_{false};
};

class PutFileStreamReactor final
    : public grpc::ServerReadReactor<zchat::FileChunk> {
  public:
    PutFileStreamReactor(std::shared_ptr<FileApplicationService> service,
                         zchat::PutSingleFileRsp *response)
        : service_(std::move(service)), response_(response) {
        StartRead(&chunk_);
    }

    void OnReadDone(bool ok) override {
        if (!ok) {
            FinishStore();
            return;
        }
        if (first_) {
            file_name_ = chunk_.file_name();
            file_size_ = chunk_.file_size();
            first_ = false;
        }
        content_.append(chunk_.chunk_data());
        StartRead(&chunk_);
    }

    void OnDone() override { delete this; }

    void OnCancel() override {
        cancelled_.store(true);
        SafeFinish(grpc::Status(grpc::StatusCode::CANCELLED, "cancelled"));
    }

  private:
    void SafeFinish(const grpc::Status &status) {
        if (!finished_.exchange(true)) {
            Finish(status);
        }
    }

    void FinishStore() {
        if (cancelled_.load()) {
            SafeFinish(grpc::Status(grpc::StatusCode::CANCELLED, "cancelled"));
            return;
        }
        [](PutFileStreamReactor *self) -> drogon::AsyncTask {
            if (self->cancelled_.load()) {
                self->SafeFinish(
                    grpc::Status(grpc::StatusCode::CANCELLED, "cancelled"));
                co_return;
            }
            auto file_id = co_await self->service_->StoreFileContentCoro(
                self->file_name_, static_cast<std::uint64_t>(self->file_size_),
                self->content_);
            if (!file_id.ok()) {
                self->response_->set_success(false);
                self->response_->set_errmsg(
                    FormatErrorForClient(file_id.error()));
            } else {
                self->response_->set_success(true);
                self->response_->set_errmsg("");
                self->response_->mutable_file_info()->set_file_id(
                    file_id.value());
                self->response_->mutable_file_info()->set_file_name(
                    self->file_name_);
                self->response_->mutable_file_info()->set_file_size(
                    self->file_size_);
            }
            self->SafeFinish(grpc::Status::OK);
            co_return;
        }(this);
    }

    std::shared_ptr<FileApplicationService> service_;
    zchat::PutSingleFileRsp *response_;
    zchat::FileChunk chunk_;
    std::string file_name_;
    std::int64_t file_size_ = 0;
    std::string content_;
    bool first_ = true;
    std::atomic_bool finished_{false};
    std::atomic_bool cancelled_{false};
};

} // namespace

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

grpc::ServerWriteReactor<zchat::FileChunk> *
FileGrpcService::GetSingleFileStream(grpc::CallbackServerContext *,
                                     const zchat::GetSingleFileReq *request) {
    ZCHAT_LOG_INFO("FileService::GetSingleFileStream request_id={}",
                   request->request_id());
    return new GetFileStreamReactor(service_, *request);
}

grpc::ServerReadReactor<zchat::FileChunk> *
FileGrpcService::PutSingleFileStream(grpc::CallbackServerContext *,
                                     zchat::PutSingleFileRsp *response) {
    ZCHAT_LOG_INFO("FileService::PutSingleFileStream");
    return new PutFileStreamReactor(service_, response);
}

} // namespace zchat
