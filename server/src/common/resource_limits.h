#ifndef ZCHAT_SERVER_SRC_COMMON_RESOURCE_LIMITS_H_
#define ZCHAT_SERVER_SRC_COMMON_RESOURCE_LIMITS_H_

#include <cstdint>
#include <mutex>

namespace zchat::resource_limits {

inline constexpr int kLoginPerMinute = 60;
inline constexpr int kRegisterPerMinute = 10;
inline constexpr int kRegisterPerDay = 1000;
inline constexpr int kSmsPerMinute = 10;
inline constexpr int kSmsPerDay = 100;
inline constexpr int kUploadPerMinute = 60;
inline constexpr std::uint64_t kFileBytes = 16 * 1024 * 1024;
inline constexpr std::uint64_t kAvatarBytes = 1024 * 1024;
inline constexpr std::uint64_t kUserBytes = 256 * 1024 * 1024;
inline constexpr std::uint64_t kGlobalBytes = 10ULL * 1024 * 1024 * 1024;
inline constexpr std::uint64_t kUserFiles = 1000;
inline constexpr std::uint64_t kGlobalFiles = 10000;

// Only held during synchronous password computation, never across co_await.
inline std::mutex password_computation;

} // namespace zchat::resource_limits

#endif
