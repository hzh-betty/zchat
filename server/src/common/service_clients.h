#ifndef ZCHAT_SERVER_SRC_COMMON_SERVICE_CLIENTS_H_
#define ZCHAT_SERVER_SRC_COMMON_SERVICE_CLIENTS_H_

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include <grpcpp/grpcpp.h>

#include <drogon/utils/coroutine.h>

#include "common/channel_pool.h"
#include "common/config.h"
#include "common/domain_records.h"
#include "common/etcd_service.h"
#include "common/grpc_awaiter.h"
#include "common/result.h"
#include "file.grpc.pb.h"
#include "friend.grpc.pb.h"
#include "message.grpc.pb.h"
#include "user.grpc.pb.h"

namespace zchat {

class ServiceClients : public NonCopyable {
  public:
    explicit ServiceClients(const EtcdConfig &config);

    drogon::Task<Result<zchat::GetUserInfoRsp>>
    GetUserCoro(const zchat::GetUserInfoReq &request);
    drogon::Task<Result<zchat::GetMultiUserInfoRsp>>
    GetMultiUserInfoCoro(const zchat::GetMultiUserInfoReq &request);
    drogon::Task<Result<zchat::SearchUsersRsp>>
    SearchUsersCoro(const zchat::SearchUsersReq &request);
    drogon::Task<Result<zchat::GetChatSessionMemberIdsRsp>>
    GetChatSessionMemberIdsCoro(
        const zchat::GetChatSessionMemberIdsReq &request);
    drogon::Task<Result<zchat::GetRecentMsgRsp>>
    GetRecentMsgCoro(const zchat::GetRecentMsgReq &request);
    drogon::Task<Result<zchat::GetMultiRecentMsgRsp>>
    GetMultiRecentMsgCoro(const zchat::GetMultiRecentMsgReq &request);
    drogon::Task<Result<std::optional<FileRecord>>>
    GetFileCoro(const std::string &file_id,
                const std::string &caller_user_id = "");
    drogon::Task<Result<zchat::GetMultiFileRsp>>
    GetMultiFileCoro(const std::vector<std::string> &file_ids,
                     const std::string &caller_user_id = "");
    drogon::Task<Result<std::string>>
    PutFileCoro(const std::string &file_name, const std::string &file_content,
                const std::string &owner_user_id = "",
                const std::string &chat_session_id = "");

  private:
    static constexpr auto kGrpcDeadline = std::chrono::seconds(5);
    static constexpr auto kFileGrpcDeadline = std::chrono::seconds(30);

    EtcdDiscovery discovery_;
    ChannelPool channel_pool_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_COMMON_SERVICE_CLIENTS_H_
