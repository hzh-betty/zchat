#include "speech/speech_recognizer.h"

#include <utility>

namespace zchat {

ConfiguredSpeechRecognizer::ConfiguredSpeechRecognizer(
    bool enabled, std::string placeholder_result)
    : enabled_(enabled), placeholder_result_(std::move(placeholder_result)) {}

Result<std::string> ConfiguredSpeechRecognizer::Recognize(const std::string &) {
    if (!enabled_) {
        return Result<std::string>::Ok(placeholder_result_);
    }
    return Result<std::string>::Fail(
        "百度语音 SDK 未接入，请填写配置并安装 SDK 后启用");
}

} // namespace zchat
