#ifndef ZCHAT_SERVER_SRC_COMMON_UUID_H_
#define ZCHAT_SERVER_SRC_COMMON_UUID_H_

#include <cstdint>
#include <string>

namespace zchat {

std::string NewId();
std::string NewRequestId();
std::int64_t UnixTimeSeconds();

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_COMMON_UUID_H_
