#ifndef PEAR_CLI_COMMANDS_HPP
#define PEAR_CLI_COMMANDS_HPP

#include <filesystem>
#include <string>
#include <vector>

namespace pear::cli {

void run_init(const std::filesystem::path& workspace_path);
void run_deinit();

void run_connect(const std::string& gu_address, const std::string& listen_address, bool is_main);
void run_disconnect();

void run_add(const std::vector<std::filesystem::path>& paths, bool all);
void run_unstage(const std::vector<std::filesystem::path>& paths, bool all);

void run_update();
void run_ls(bool json_format);
void run_push();

void run_pull(const std::vector<std::string>& targets);
void run_status(bool json_format);
void run_log(size_t tail);
void run_cleanup(size_t keep_versions, bool dry_run);

} // namespace pear::cli

#endif  // PEAR_CLI_COMMANDS_HPP
