#ifndef ZCHAT_SERVER_SRC_COMMON_LOGGER_H_
#define ZCHAT_SERVER_SRC_COMMON_LOGGER_H_

#include <memory>
#include <string>
#include <string_view>

#include <spdlog/spdlog.h>

#include "common/config.h"

namespace zchat {

void InitLogger(const std::string &service_name, const LogConfig &config);
std::shared_ptr<spdlog::logger> Logger();
void FlushLogger();

std::string RedactPhone(std::string_view phone);
std::string RedactToken(std::string_view token);
std::string RedactSecret(std::string_view value);
std::string RedactPassword();

} // namespace zchat

#define ZCHAT_LOG_TRACE(...)                                                   \
    do {                                                                       \
        zchat::Logger()->trace("[{}:{}] {}", __FILE__, __LINE__,               \
                               spdlog::fmt_lib::format(__VA_ARGS__));          \
    } while (false)

#define ZCHAT_LOG_DEBUG(...)                                                   \
    do {                                                                       \
        zchat::Logger()->debug("[{}:{}] {}", __FILE__, __LINE__,               \
                               spdlog::fmt_lib::format(__VA_ARGS__));          \
    } while (false)

#define ZCHAT_LOG_INFO(...)                                                    \
    do {                                                                       \
        zchat::Logger()->info("[{}:{}] {}", __FILE__, __LINE__,                \
                              spdlog::fmt_lib::format(__VA_ARGS__));           \
    } while (false)

#define ZCHAT_LOG_WARN(...)                                                    \
    do {                                                                       \
        zchat::Logger()->warn("[{}:{}] {}", __FILE__, __LINE__,                \
                              spdlog::fmt_lib::format(__VA_ARGS__));           \
    } while (false)

#define ZCHAT_LOG_ERROR(...)                                                   \
    do {                                                                       \
        zchat::Logger()->error("[{}:{}] {}", __FILE__, __LINE__,               \
                               spdlog::fmt_lib::format(__VA_ARGS__));          \
    } while (false)

#define ZCHAT_LOG_FATAL(...)                                                   \
    do {                                                                       \
        zchat::Logger()->critical("[{}:{}] {}", __FILE__, __LINE__,            \
                                  spdlog::fmt_lib::format(__VA_ARGS__));       \
    } while (false)

#endif // ZCHAT_SERVER_SRC_COMMON_LOGGER_H_
