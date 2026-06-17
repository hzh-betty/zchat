#ifndef ZCHAT_SERVER_SRC_COMMON_SERVICE_CLIENTS_H_
#define ZCHAT_SERVER_SRC_COMMON_SERVICE_CLIENTS_H_

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include <grpcpp/grpcpp.h>

#include "common/channel_pool.h"
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

    Result<zchat::SearchUsersRsp>
    SearchUsers(const zchat::SearchUsersReq &request);

    Result<zchat::GetChatSessionMemberIdsRsp>
    GetChatSessionMemberIds(const zchat::GetChatSessionMemberIdsReq &request);

    Result<zchat::GetRecentMsgRsp>
    GetRecentMsg(const zchat::GetRecentMsgReq &request);

    Result<std::optional<FileRecord>> GetFile(const std::string &file_id);

    Result<zchat::GetMultiFileRsp>
    GetMultiFile(const std::vector<std::string> &file_ids);

    Result<std::string> PutFile(const std::string &file_name,
                                const std::string &file_content);

  private:
    template <typename Service, typename Req, typename Rsp>
    Result<Rsp>
    CallUnary(const char *service_name, std::chrono::seconds deadline,
              grpc::Status (Service::Stub::*method)(grpc::ClientContext *,
                                                    const Req &, Rsp *),
              const Req &request);

    static constexpr auto kGrpcDeadline = std::chrono::seconds(5);
    static constexpr auto kFileGrpcDeadline = std::chrono::seconds(30);

    EtcdDiscovery discovery_;
    ChannelPool channel_pool_;
};

template <typename Service, typename Req, typename Rsp>
Result<Rsp> ServiceClients::CallUnary(
    const char *service_name, std::chrono::seconds deadline,
    grpc::Status (Service::Stub::*method)(grpc::ClientContext *, const Req &,
                                          Rsp *),
    const Req &request) {
    auto endpoint = discovery_.Endpoint(service_name);
    if (!endpoint.ok()) {
        return Result<Rsp>::Fail(endpoint.error());
    }
    auto stub =
        Service::NewStub(channel_pool_.GetOrCreateChannel(endpoint.value()));
    Rsp response;
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + deadline);
    const grpc::Status status =
        (stub.get()->*method)(&context, request, &response);
    if (!status.ok()) {
        return Result<Rsp>::Fail(
            AppError::WithCode(ErrorCode::kExternalServiceError,
                               std::string(service_name) + " grpc call failed")
                .WithDetail(status.error_message()));
    }
    return Result<Rsp>::Ok(std::move(response));
}

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_COMMON_SERVICE_CLIENTS_H_
