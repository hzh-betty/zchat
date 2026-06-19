#include "common/uuid.h"

#include <chrono>
#include <cstdio>
#include <string>

#include "common/crypto.h"

namespace zchat {

std::string NewId() { return CsprngHex(16); }

std::string NewRequestId() { return "R" + CsprngHex(6); }

std::int64_t UnixTimeSeconds() {
    const auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(
               now.time_since_epoch())
        .count();
}

} // namespace zchat
