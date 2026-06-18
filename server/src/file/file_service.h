#ifndef ZCHAT_SERVER_SRC_FILE_FILE_SERVICE_H_
#define ZCHAT_SERVER_SRC_FILE_FILE_SERVICE_H_

#include "common/noncopyable.h"

#include <cstdint>
#include <optional>
#include <string>

#include <drogon/utils/coroutine.h>

#include "common/service_clients.h"
#include "common/session_store.h"
#include "file.pb.h"
#include "file/file_repository.h"

namespace zchat {

class FileApplicationService : public NonCopyable {
  public:
    explicit FileApplicationService(FileRepository &repository,
                                    ServiceClients &clients,
                                    SessionStore &sessions);
    ~FileApplicationService() = default;

    drogon::Task<zchat::GetSingleFileRsp>
    GetSingleFileCoro(const zchat::GetSingleFileReq &request);
    drogon::Task<zchat::GetMultiFileRsp>
    GetMultiFileCoro(const zchat::GetMultiFileReq &request);
    drogon::Task<zchat::PutSingleFileRsp>
    PutSingleFileCoro(const zchat::PutSingleFileReq &request);
    drogon::Task<zchat::PutMultiFileRsp>
    PutMultiFileCoro(const zchat::PutMultiFileReq &request);

    drogon::Task<Result<std::optional<FileRecord>>>
    GetFileForDownloadCoro(const std::string &file_id);
    drogon::Task<Result<std::string>>
    StoreFileContentCoro(const std::string &file_name, std::uint64_t file_size,
                         const std::string &file_content,
                         const std::string &owner_user_id = "",
                         const std::string &session_id = "");

  private:
    drogon::Task<bool> CanAccessFileCoro(const FileRecord &file,
                                         const std::string &caller_user_id);
    drogon::Task<bool>
    CheckSessionMemberCachedCoro(const std::string &session_id,
                                 const std::string &user_id);
    drogon::Task<zchat::GetSingleFileRsp>
    GetSingleFileInternal(const std::string &request_id,
                          const std::string &file_id,
                          const std::string &caller_user_id);

    FileRepository &repository_;
    ServiceClients &clients_;
    SessionStore &sessions_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_FILE_FILE_SERVICE_H_
