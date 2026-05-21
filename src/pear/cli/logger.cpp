#include <pear/cli/logger.hpp>

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace pear::cli {

namespace {

std::string make_timestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t current_time = std::chrono::system_clock::to_time_t(now);

    std::tm tm_snapshot {};
#if defined(_WIN32)
    localtime_s(&tm_snapshot, &current_time);
#else
    localtime_r(&current_time, &tm_snapshot);
#endif

    std::ostringstream buffer;
    buffer << std::put_time(&tm_snapshot, "%Y-%m-%d %H:%M:%S");
    return buffer.str();
}

const char* level_name(LogLevel level) {
    switch (level) {
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Error:
            return "ERROR";
    }

    return "INFO";
}

} // namespace

void log_event(const std::filesystem::path& workspace_root, LogLevel level, const std::string& command, const std::string& message) {
    try {
        const std::filesystem::path log_path = workspace_root / ".peer" / "meta" / "pear.log";
        std::ofstream output(log_path, std::ios::app);

        if (!output.is_open()) {
            return;
        }

        output << '[' << make_timestamp() << "] " << level_name(level) << ' ' << command << ' ' << message << '\n';
    } catch (...) {
    }
}

void log_info(const std::filesystem::path& workspace_root, const std::string& command, const std::string& message) {
    log_event(workspace_root, LogLevel::Info, command, message);
}

void log_error(const std::filesystem::path& workspace_root, const std::string& command, const std::string& message) {
    log_event(workspace_root, LogLevel::Error, command, message);
}

} // namespace pear::cli
