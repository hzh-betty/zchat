#ifndef ZCHAT_SERVER_SRC_FILE_FILE_SERVICE_H_
#define ZCHAT_SERVER_SRC_FILE_FILE_SERVICE_H_

#include "common/noncopyable.h"

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

  private:
    FileRepository &repository_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_FILE_FILE_SERVICE_H_
