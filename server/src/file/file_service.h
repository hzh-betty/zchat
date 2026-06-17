#ifndef ZCHAT_SERVER_SRC_FILE_FILE_SERVICE_H_
#define ZCHAT_SERVER_SRC_FILE_FILE_SERVICE_H_

#include "common/noncopyable.h"

#include <cstdint>
#include <optional>
#include <string>

#include "file.pb.h"
#include "file/file_repository.h"

namespace zchat {

class FileApplicationService : public NonCopyable {
  public:
    explicit FileApplicationService(FileRepository &repository);

    ~FileApplicationService() = default;

    zchat::GetSingleFileRsp
    GetSingleFile(const zchat::GetSingleFileReq &request);
    zchat::GetMultiFileRsp GetMultiFile(const zchat::GetMultiFileReq &request);
    zchat::PutSingleFileRsp
    PutSingleFile(const zchat::PutSingleFileReq &request);
    zchat::PutMultiFileRsp PutMultiFile(const zchat::PutMultiFileReq &request);

    Result<std::optional<FileRecord>>
    GetFileForDownload(const std::string &file_id);
    Result<std::string> StoreFileContent(const std::string &file_name,
                                         std::uint64_t file_size,
                                         const std::string &file_content);

  private:
    FileRepository &repository_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_FILE_FILE_SERVICE_H_
