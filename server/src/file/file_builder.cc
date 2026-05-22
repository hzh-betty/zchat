#include "file/file_builder.h"

#include "common/runtime.h"

namespace zchat {

FileBuilder::FileBuilder(const AppConfig &config) : config_(config) {}

int FileBuilder::Start() {
    context_ = std::make_unique<FileContext>(config_);
    return RunGrpcServer("zchat_file_service", config_.services.file,
                         &context_->grpc_service());
}

} // namespace zchat
