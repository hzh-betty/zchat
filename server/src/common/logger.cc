#include "common/logger.h"

#include <filesystem>
#include <mutex>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace zchat {
namespace {

std::mutex g_logger_mutex;
std::shared_ptr<spdlog::logger> g_logger;

spdlog::level::level_enum ParseLogLevel(const std::string &level) {
    const auto parsed = spdlog::level::from_str(level);
    if (parsed == spdlog::level::off && level != "off") {
        return spdlog::level::debug;
    }
    return parsed;
}

} // namespace

void InitLogger(const std::string &service_name, bool console,
                const std::string &log_file, const std::string &level) {
    std::lock_guard<std::mutex> lock(g_logger_mutex);
    if (g_logger != nullptr && g_logger->name() == service_name) {
        return;
    }

    spdlog::drop(service_name);
    if (console || log_file.empty()) {
        g_logger = spdlog::stdout_color_mt(service_name);
    } else {
        const std::filesystem::path path(log_file);
        if (!path.parent_path().empty()) {
            std::filesystem::create_directories(path.parent_path());
        }
        g_logger = spdlog::basic_logger_mt(service_name, log_file, true);
    }
    g_logger->set_level(ParseLogLevel(level));
    g_logger->set_pattern("[%n][%Y-%m-%d %H:%M:%S.%e][%t][%-8l]%v");
    g_logger->flush_on(spdlog::level::warn);
    spdlog::set_default_logger(g_logger);
}

std::shared_ptr<spdlog::logger> Logger() {
    {
        std::lock_guard<std::mutex> lock(g_logger_mutex);
        if (g_logger != nullptr) {
            return g_logger;
        }
    }
    InitLogger("zchat");
    std::lock_guard<std::mutex> lock(g_logger_mutex);
    return g_logger;
}

void FlushLogger() {
    auto logger = Logger();
    logger->flush();
}

} // namespace zchat
