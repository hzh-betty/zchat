#include "common/uuid.h"

#include <chrono>
#include <iomanip>
#include <random>
#include <sstream>

namespace zchat {
namespace {

std::mt19937_64 &RandomEngine() {
    static std::random_device device;
    static std::mt19937_64 engine(device());
    return engine;
}

std::string RandomHex(std::size_t bytes) {
    std::uniform_int_distribution<unsigned int> distribution(0, 255);
    std::ostringstream stream;
    for (std::size_t i = 0; i < bytes; ++i) {
        stream << std::hex << std::setw(2) << std::setfill('0')
               << distribution(RandomEngine());
    }
    return stream.str();
}

} // namespace

std::string NewId() { return RandomHex(16); }

std::string NewRequestId() { return "R" + RandomHex(6); }

std::string NewVerifyCode() {
    std::uniform_int_distribution<int> distribution(0, 999999);
    std::ostringstream stream;
    stream << std::setw(6) << std::setfill('0') << distribution(RandomEngine());
    return stream.str();
}

std::int64_t UnixTimeSeconds() {
    const auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(
               now.time_since_epoch())
        .count();
}

} // namespace zchat
