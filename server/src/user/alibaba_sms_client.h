#ifndef ZCHAT_SERVER_SRC_USER_ALIBABA_SMS_CLIENT_H_
#define ZCHAT_SERVER_SRC_USER_ALIBABA_SMS_CLIENT_H_

#include "common/noncopyable.h"

#include <map>
#include <string>

#include <drogon/HttpClient.h>
#include <trantor/net/EventLoopThread.h>

#include "common/config.h"
#include "common/result.h"
#include "user/sms_client.h"

namespace zchat {

class AlibabaSmsClient final : public SmsClient {
  public:
    explicit AlibabaSmsClient(const SmsConfig &config);

    ~AlibabaSmsClient() override = default;

    VoidResult SendVerificationCode(const std::string &phone) override;

    VoidResult CheckVerificationCode(const std::string &phone,
                                     const std::string &code) override;

  private:
    std::string FormatUtcTimestamp() const;
    std::string ComputeSignature(
        const std::map<std::string, std::string> &params,
        const std::string &secret) const;
    std::string BuildQueryString(
        const std::map<std::string, std::string> &params) const;
    VoidResult SendRequest(const std::map<std::string, std::string> &params);

    std::string access_key_id_;
    std::string access_key_secret_;
    std::string sign_name_;
    std::string template_code_;

    std::unique_ptr<trantor::EventLoopThread> loop_thread_;
    drogon::HttpClientPtr client_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_USER_ALIBABA_SMS_CLIENT_H_