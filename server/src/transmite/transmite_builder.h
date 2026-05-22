#ifndef ZCHAT_SERVER_SRC_TRANSMITE_TRANSMITE_BUILDER_H_
#define ZCHAT_SERVER_SRC_TRANSMITE_TRANSMITE_BUILDER_H_

#include "common/noncopyable.h"

#include <memory>

#include "common/config.h"
#include "transmite/transmite_context.h"

namespace zchat {

class TransmiteBuilder : public NonCopyable {
  public:
    explicit TransmiteBuilder(const AppConfig &config);

    ~TransmiteBuilder() = default;

    int Start();

  private:
    AppConfig config_;
    std::unique_ptr<TransmiteContext> context_;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_TRANSMITE_TRANSMITE_BUILDER_H_
