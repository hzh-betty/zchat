#ifndef ZCHAT_SERVER_SRC_SPEECH_SPEECH_BUILDER_H_
#define ZCHAT_SERVER_SRC_SPEECH_SPEECH_BUILDER_H_

#include "common/noncopyable.h"

#include <memory>

#include "common/config.h"
#include "speech/speech_context.h"

namespace zchat {

class SpeechBuilder : public NonCopyable {
  public:
    explicit SpeechBuilder(const AppConfig &config);

    ~SpeechBuilder() = default;

    int Start();

  private:
    AppConfig config_;
    std::unique_ptr<SpeechContext> context_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_SPEECH_SPEECH_BUILDER_H_
