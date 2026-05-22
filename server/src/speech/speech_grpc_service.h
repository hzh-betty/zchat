#ifndef ZCHAT_SERVER_SRC_SPEECH_SPEECH_GRPC_SERVICE_H_
#define ZCHAT_SERVER_SRC_SPEECH_SPEECH_GRPC_SERVICE_H_

#include "common/noncopyable.h"

#include <memory>

#include <grpcpp/grpcpp.h>

#include "speech.grpc.pb.h"
#include "speech/speech_service.h"

namespace zchat {

class SpeechGrpcService final : public zchat::SpeechService::Service,
                                public NonCopyable {
  public:
    explicit SpeechGrpcService(
        std::shared_ptr<SpeechApplicationService> service);

    ~SpeechGrpcService() override = default;

    grpc::Status
    SpeechRecognition(grpc::ServerContext *context,
                      const zchat::SpeechRecognitionReq *request,
                      zchat::SpeechRecognitionRsp *response) override;

  private:
    std::shared_ptr<SpeechApplicationService> service_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_SPEECH_SPEECH_GRPC_SERVICE_H_
