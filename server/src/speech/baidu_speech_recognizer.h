#ifndef ZCHAT_SERVER_SRC_SPEECH_BAIDU_SPEECH_RECOGNIZER_H_
#define ZCHAT_SERVER_SRC_SPEECH_BAIDU_SPEECH_RECOGNIZER_H_

#include "common/noncopyable.h"

#include <chrono>
#include <mutex>
#include <string>

#include <drogon/HttpClient.h>
#include <drogon/utils/coroutine.h>
#include <trantor/net/EventLoopThread.h>

#include "common/config.h"
#include "common/result.h"
#include "speech/speech_recognizer.h"

namespace zchat {

struct BaiduTokenInfo {
    std::string token;
    std::chrono::steady_clock::time_point expiry;
};

class BaiduSpeechRecognizer final : public SpeechRecognizer {
  public:
    explicit BaiduSpeechRecognizer(const SpeechConfig &config);

    ~BaiduSpeechRecognizer() override = default;

    drogon::Task<Result<std::string>>
    RecognizeCoro(const std::string &speech_data) override;

  private:
    drogon::Task<Result<BaiduTokenInfo>> FetchAccessTokenCoro();
    drogon::Task<Result<std::string>> GetAccessTokenCoro();

    std::string app_id_;
    std::string api_key_;
    std::string secret_key_;

    std::string access_token_;
    std::chrono::steady_clock::time_point token_expiry_;
    std::mutex token_mutex_;

    std::unique_ptr<trantor::EventLoopThread> loop_thread_;
    drogon::HttpClientPtr client_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_SPEECH_BAIDU_SPEECH_RECOGNIZER_H_
