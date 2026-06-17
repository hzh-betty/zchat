#ifndef ZCHAT_SERVER_SRC_COMMON_SERVICE_CLIENTS_H_
#define ZCHAT_SERVER_SRC_COMMON_SERVICE_CLIENTS_H_

#include <memory>
#include <string>
#include <vector>

#include <grpcpp/grpcpp.h>

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/config.h"
#include "common/domain_records.h"
#include "common/etcd_service.h"
#include "common/result.h"
#include "file.grpc.pb.h"
#include "friend.grpc.pb.h"
#include "message.grpc.pb.h"
#include "user.grpc.pb.h"

namespace zchat {

class ServiceClients : public NonCopyable {
  public:
    explicit ServiceClients(const EtcdConfig &config);

    Result<zchat::GetUserInfoRsp> GetUser(const zchat::GetUserInfoReq &request);

    Result<zchat::GetMultiUserInfoRsp>
    GetMultiUserInfo(const zchat::GetMultiUserInfoReq &request);

    Result<zchat::GetChatSessionMemberIdsRsp>
    GetChatSessionMemberIds(const zchat::GetChatSessionMemberIdsReq &request);

    Result<zchat::GetRecentMsgRsp>
    GetRecentMsg(const zchat::GetRecentMsgReq &request);

    Result<std::optional<FileRecord>> GetFile(const std::string &file_id);

    Result<std::string> PutFile(const std::string &file_name,
                                const std::string &file_content);

  private:
    std::shared_ptr<grpc::Channel>
    GetOrCreateChannel(const std::string &endpoint);

    static constexpr auto kGrpcDeadline = std::chrono::seconds(5);
    static constexpr auto kFileGrpcDeadline = std::chrono::seconds(30);

    EtcdDiscovery discovery_;
    std::mutex channel_mutex_;
    std::unordered_map<std::string, std::shared_ptr<grpc::Channel>> channels_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_COMMON_SERVICE_CLIENTS_H_