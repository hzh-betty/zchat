#ifndef ZCHAT_SERVER_SRC_SPEECH_BAIDU_SPEECH_RECOGNIZER_H_
#define ZCHAT_SERVER_SRC_SPEECH_BAIDU_SPEECH_RECOGNIZER_H_

#include "common/noncopyable.h"

#include <chrono>
#include <mutex>
#include <string>

#include <drogon/HttpClient.h>
#include <trantor/net/EventLoopThread.h>

#include "common/config.h"
#include "common/result.h"
#include "speech/speech_recognizer.h"

namespace zchat {

class BaiduSpeechRecognizer final : public SpeechRecognizer {
  public:
    explicit BaiduSpeechRecognizer(const SpeechConfig &config);

    ~BaiduSpeechRecognizer() override = default;

    Result<std::string> Recognize(const std::string &speech_data) override;

  private:
    Result<std::string> FetchAccessToken();
    Result<std::string> GetAccessToken();

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