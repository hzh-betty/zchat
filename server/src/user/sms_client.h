#ifndef ZCHAT_SERVER_SRC_USER_SMS_CLIENT_H_
#define ZCHAT_SERVER_SRC_USER_SMS_CLIENT_H_

#include "common/noncopyable.h"

#include <string>

#include <drogon/utils/coroutine.h>

#include "common/result.h"

namespace zchat {

class SmsClient : public NonCopyable {
  public:
    SmsClient() = default;

    virtual ~SmsClient() = default;

    virtual drogon::Task<VoidResult>
    SendVerificationCode(const std::string &phone) = 0;

    virtual drogon::Task<VoidResult>
    CheckVerificationCode(const std::string &phone,
                          const std::string &code) = 0;
};

class ConfiguredSmsClient final : public SmsClient {
  public:
    ConfiguredSmsClient() = default;

    ~ConfiguredSmsClient() override = default;

    drogon::Task<VoidResult>
    SendVerificationCode(const std::string &phone) override;

    drogon::Task<VoidResult>
    CheckVerificationCode(const std::string &phone,
                          const std::string &code) override;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_USER_SMS_CLIENT_H_
