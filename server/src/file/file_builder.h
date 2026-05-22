#ifndef ZCHAT_SERVER_SRC_FILE_FILE_BUILDER_H_
#define ZCHAT_SERVER_SRC_FILE_FILE_BUILDER_H_

#include "common/noncopyable.h"

#include <memory>

#include "common/config.h"
#include "file/file_context.h"

namespace zchat {

class FileBuilder : public NonCopyable {
  public:
    explicit FileBuilder(const AppConfig &config);

    ~FileBuilder() = default;

    int Start();

  private:
    AppConfig config_;
    std::unique_ptr<FileContext> context_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_FILE_FILE_BUILDER_H_
