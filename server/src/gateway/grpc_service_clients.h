#ifndef ZCHAT_SERVER_SRC_GATEWAY_GRPC_SERVICE_CLIENTS_H_
#define ZCHAT_SERVER_SRC_GATEWAY_GRPC_SERVICE_CLIENTS_H_

#include "common/noncopyable.h"

#include <functional>
#include <memory>
#include <string>

#include <drogon/HttpResponse.h>

#include "common/config.h"
#include "file.grpc.pb.h"
#include "friend.grpc.pb.h"
#include "message.grpc.pb.h"
#include "speech.grpc.pb.h"
#include "transmite.grpc.pb.h"
#include "user.grpc.pb.h"

namespace zchat {

class GrpcServiceClients : public NonCopyable {
  public:
    explicit GrpcServiceClients(const AppConfig &config);

    ~GrpcServiceClients() = default;

    void
    Forward(const std::string &path, const std::string &body,
            std::function<void(const drogon::HttpResponsePtr &)> &&callback);

  private:
    std::unique_ptr<zchat::UserService::Stub> user_;
    std::unique_ptr<zchat::FriendService::Stub> friend_;
    std::unique_ptr<zchat::MsgStorageService::Stub> message_;
    std::unique_ptr<zchat::MsgTransmitService::Stub> transmite_;
    std::unique_ptr<zchat::FileService::Stub> file_;
    std::unique_ptr<zchat::SpeechService::Stub> speech_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_GATEWAY_GRPC_SERVICE_CLIENTS_H_
