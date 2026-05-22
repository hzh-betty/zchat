#include "common/logger.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main() {
    const auto log_path =
        std::filesystem::temp_directory_path() / "zchat_logger_test.log";
    std::filesystem::remove(log_path);

    zchat::InitLogger("logger_test", false, log_path.string(), "debug");
    ZCHAT_LOG_INFO("logger smoke {}", 42);
    zchat::FlushLogger();

    std::ifstream input(log_path);
    std::string content((std::istreambuf_iterator<char>(input)),
                        std::istreambuf_iterator<char>());
    if (content.find("logger smoke 42") == std::string::npos) {
        std::cerr << "missing log content in " << log_path << '\n';
        return 1;
    }
    std::filesystem::remove(log_path);
    return 0;
}
