#ifndef ZCHAT_SERVER_SRC_TRANSMITE_TRANSMITE_GRPC_SERVICE_H_
#define ZCHAT_SERVER_SRC_TRANSMITE_TRANSMITE_GRPC_SERVICE_H_

#include "common/noncopyable.h"

#include <memory>

#include <grpcpp/grpcpp.h>

#include "transmite.grpc.pb.h"
#include "transmite/transmite_service.h"

namespace zchat {

class TransmiteGrpcService final
    : public zchat::MsgTransmitService::CallbackService,
      public NonCopyable {
  public:
    explicit TransmiteGrpcService(std::shared_ptr<TransmiteService> service);
    ~TransmiteGrpcService() override = default;

    grpc::ServerUnaryReactor *
    GetTransmitTarget(grpc::CallbackServerContext *context,
                      const zchat::NewMessageReq *request,
                      zchat::GetTransmitTargetRsp *response) override;

  private:
    std::shared_ptr<TransmiteService> service_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_TRANSMITE_TRANSMITE_GRPC_SERVICE_H_
