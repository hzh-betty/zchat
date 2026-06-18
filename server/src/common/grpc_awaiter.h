#ifndef ZCHAT_SERVER_SRC_COMMON_GRPC_AWAITER_H_
#define ZCHAT_SERVER_SRC_COMMON_GRPC_AWAITER_H_

#include <chrono>
#include <coroutine>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include <drogon/utils/coroutine.h>
#include <grpcpp/grpcpp.h>

#include "common/channel_pool.h"
#include "common/common_errors.h"
#include "common/etcd_service.h"
#include "common/result.h"

namespace zchat {

// 通用协程化 gRPC unary 调用。
// async_call: 可调用对象，签名
//   void(Stub*, ClientContext*, const Req*, Rsp*,
//        std::function<void(grpc::Status)>)
template <typename Service, typename Req, typename Rsp, typename AsyncCall>
drogon::Task<Result<Rsp>>
CallUnaryCoro(EtcdDiscovery &discovery, ChannelPool &channel_pool,
              const char *service_name, AsyncCall async_call,
              const Req &request, std::chrono::seconds deadline) {
    auto endpoint = discovery.Endpoint(service_name);
    if (!endpoint.ok()) {
        co_return Result<Rsp>::Fail(endpoint.error());
    }

    auto stub =
        Service::NewStub(channel_pool.GetOrCreateChannel(endpoint.value()));
    auto stub_ptr = std::shared_ptr<typename Service::Stub>(std::move(stub));
    auto response = std::make_shared<Rsp>();
    auto context = std::make_shared<grpc::ClientContext>();
    context->set_deadline(std::chrono::system_clock::now() + deadline);
    Req req_copy = request;

    // GrpcAwaiter 持有所有资源，在 await_suspend 中发起 async 调用，
    // 回调中 set value 并 resume 协程。
    struct GrpcAwaiter : public drogon::CallbackAwaiter<Result<Rsp>> {
        std::shared_ptr<typename Service::Stub> stub;
        std::shared_ptr<Rsp> response;
        std::shared_ptr<grpc::ClientContext> context;
        AsyncCall async_call;
        Req request;

        GrpcAwaiter(std::shared_ptr<typename Service::Stub> s,
                    std::shared_ptr<Rsp> r,
                    std::shared_ptr<grpc::ClientContext> c, AsyncCall a,
                    Req req)
            : stub(std::move(s)), response(std::move(r)), context(std::move(c)),
              async_call(std::move(a)), request(std::move(req)) {}

        void await_suspend(std::coroutine_handle<> handle) {
            async_call(
                stub.get(), context.get(), &request, response.get(),
                [this, handle](grpc::Status status) {
                    if (status.ok()) {
                        this->setValue(Result<Rsp>::Ok(std::move(*response)));
                    } else {
                        this->setValue(Result<Rsp>::Fail(
                            AppError::WithCode(ErrorCode::kExternalServiceError,
                                               "grpc call failed")
                                .WithDetail(status.error_message())));
                    }
                    handle.resume();
                });
        }
    };

    co_return co_await GrpcAwaiter(std::move(stub_ptr), std::move(response),
                                   std::move(context), std::move(async_call),
                                   std::move(req_copy));
}

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_COMMON_GRPC_AWAITER_H_
