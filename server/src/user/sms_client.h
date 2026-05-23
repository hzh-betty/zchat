#ifndef ZCHAT_SERVER_SRC_USER_SMS_CLIENT_H_
#define ZCHAT_SERVER_SRC_USER_SMS_CLIENT_H_

#include "common/noncopyable.h"

#include <string>

#include "common/result.h"

namespace zchat {

class SmsClient : public NonCopyable {
  public:
    SmsClient() = default;

    virtual ~SmsClient() = default;

    virtual VoidResult SendVerificationCode(const std::string &phone) = 0;

    virtual VoidResult CheckVerificationCode(const std::string &phone,
                                             const std::string &code) = 0;
};

class ConfiguredSmsClient final : public SmsClient {
  public:
    explicit ConfiguredSmsClient(bool enabled);

    ~ConfiguredSmsClient() override = default;

    VoidResult SendVerificationCode(const std::string &phone) override;

    VoidResult CheckVerificationCode(const std::string &phone,
                                     const std::string &code) override;

  private:
    bool enabled_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_USER_SMS_CLIENT_H_