#include "common/logger.h"

#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

#include <spdlog/sinks/rotating_file_sink.h>
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

void InitLogger(const std::string &service_name, const LogConfig &config) {
    std::lock_guard<std::mutex> lock(g_logger_mutex);
    if (g_logger != nullptr && g_logger->name() == service_name) {
        return;
    }

    spdlog::drop(service_name);

    std::vector<spdlog::sink_ptr> sinks;
    if (config.console) {
        sinks.push_back(
            std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    }
    if (config.file && !config.file_path.empty()) {
        const std::filesystem::path path(config.file_path);
        if (!path.parent_path().empty()) {
            std::filesystem::create_directories(path.parent_path());
        }
        sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            config.file_path, config.max_file_size, config.max_files));
    }
    if (sinks.empty()) {
        sinks.push_back(
            std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    }

    g_logger = std::make_shared<spdlog::logger>(service_name, sinks.begin(),
                                                sinks.end());
    g_logger->set_level(ParseLogLevel(config.level));
    g_logger->set_pattern(config.format);
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
    InitLogger("zchat", LogConfig{});
    std::lock_guard<std::mutex> lock(g_logger_mutex);
    return g_logger;
}

void FlushLogger() {
    auto logger = Logger();
    logger->flush();
}

std::string RedactPhone(std::string_view phone) {
    const auto n = phone.size();
    if (n <= 3) {
        return std::string(n, '*');
    }
    if (n < 7) {
        return std::string(phone.substr(0, 1)) + std::string(n - 2, '*') +
               std::string(phone.substr(n - 1));
    }
    return std::string(phone.substr(0, 3)) + std::string(n - 7, '*') +
           std::string(phone.substr(n - 4));
}

std::string RedactToken(std::string_view token) {
    const auto n = token.size();
    if (n <= 8) {
        return std::string(n, '*');
    }
    return std::string(token.substr(0, 4)) + std::string(n - 8, '*') +
           std::string(token.substr(n - 4));
}

std::string RedactSecret(std::string_view value) {
    const auto n = value.size();
    if (n <= 4) {
        return std::string(n, '*');
    }
    return std::string(value.substr(0, 2)) + std::string(n - 4, '*') +
           std::string(value.substr(n - 2));
}

std::string RedactPassword() { return "***"; }

} // namespace zchat
