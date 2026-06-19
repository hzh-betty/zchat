#ifndef ZCHAT_SERVER_SRC_COMMON_GRPC_CALLBACK_H_
#define ZCHAT_SERVER_SRC_COMMON_GRPC_CALLBACK_H_

#include <atomic>
#include <exception>
#include <functional>
#include <utility>

#include <drogon/utils/coroutine.h>
#include <grpcpp/grpcpp.h>

#include "common/error_response.h"
#include "common/logger.h"

namespace zchat {

template <typename Rsp>
class CoroUnaryReactor final : public grpc::ServerUnaryReactor {
  public:
    using CoroFactory = std::function<drogon::Task<Rsp>()>;

    CoroUnaryReactor(CoroFactory factory, Rsp *response,
                     const char *service_name, const char *method_name,
                     const std::string &request_id)
        : response_(response), service_name_(service_name),
          method_name_(method_name), request_id_(request_id) {
        [](CoroUnaryReactor *self, CoroFactory factory) -> drogon::AsyncTask {
            if (self->finished_.exchange(true)) {
                co_return;
            }
            try {
                *self->response_ = co_await factory();
                if (self->cancelled_.load()) {
                    co_return;
                }
                LogBoundaryResponseError(self->service_name_,
                                         self->method_name_, self->request_id_,
                                         *self->response_);
                self->SafeFinish(grpc::Status::OK);
            } catch (const std::exception &e) {
                ZCHAT_LOG_ERROR("{}::{} coroutine exception request_id={} "
                                "error={}",
                                self->service_name_, self->method_name_,
                                self->request_id_, e.what());
                self->SafeFinish(
                    grpc::Status(grpc::StatusCode::INTERNAL, "internal error"));
            }
            co_return;
        }(this, std::move(factory));
    }

    void OnDone() override { delete this; }

    void OnCancel() override {
        ZCHAT_LOG_WARN("{}::{} cancelled request_id={}", service_name_,
                       method_name_, request_id_);
        cancelled_.store(true);
        SafeFinish(grpc::Status(grpc::StatusCode::CANCELLED, "cancelled"));
    }

  private:
    void SafeFinish(const grpc::Status &status) {
        if (!finished_.exchange(true)) {
            Finish(status);
        }
    }

    Rsp *response_;
    const char *service_name_;
    const char *method_name_;
    std::string request_id_;
    std::atomic_bool finished_{false};
    std::atomic_bool cancelled_{false};
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_COMMON_GRPC_CALLBACK_H_
