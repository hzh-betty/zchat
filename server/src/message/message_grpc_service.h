#ifndef ZCHAT_SERVER_SRC_MESSAGE_MESSAGE_GRPC_SERVICE_H_
#define ZCHAT_SERVER_SRC_MESSAGE_MESSAGE_GRPC_SERVICE_H_

#include "common/noncopyable.h"

#include <memory>

#include <grpcpp/grpcpp.h>

#include "message.grpc.pb.h"
#include "message/message_service.h"

namespace zchat {

class MessageGrpcService final
    : public zchat::MsgStorageService::CallbackService,
      public NonCopyable {
  public:
    explicit MessageGrpcService(std::shared_ptr<MessageService> service);
    ~MessageGrpcService() override = default;

    grpc::ServerUnaryReactor *
    GetHistoryMsg(grpc::CallbackServerContext *context,
                  const zchat::GetHistoryMsgReq *request,
                  zchat::GetHistoryMsgRsp *response) override;
    grpc::ServerUnaryReactor *
    GetRecentMsg(grpc::CallbackServerContext *context,
                 const zchat::GetRecentMsgReq *request,
                 zchat::GetRecentMsgRsp *response) override;
    grpc::ServerUnaryReactor *
    GetMultiRecentMsg(grpc::CallbackServerContext *context,
                      const zchat::GetMultiRecentMsgReq *request,
                      zchat::GetMultiRecentMsgRsp *response) override;
    grpc::ServerUnaryReactor *MsgSearch(grpc::CallbackServerContext *context,
                                        const zchat::MsgSearchReq *request,
                                        zchat::MsgSearchRsp *response) override;

  private:
    std::shared_ptr<MessageService> service_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_MESSAGE_MESSAGE_GRPC_SERVICE_H_
