#ifndef PEAR_CLI_LOGGER_HPP
#define PEAR_CLI_LOGGER_HPP

#include <filesystem>
#include <string>

namespace pear::cli {

enum class LogLevel {
    Info,
    Error
};

void log_event(const std::filesystem::path& workspace_root, LogLevel level, const std::string& command, const std::string& message);
void log_info(const std::filesystem::path& workspace_root, const std::string& command, const std::string& message);
void log_error(const std::filesystem::path& workspace_root, const std::string& command, const std::string& message);

} // namespace pear::cli

#endif // PEAR_CLI_LOGGER_HPP
