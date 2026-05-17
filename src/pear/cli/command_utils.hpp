#ifndef PEAR_CLI_COMMAND_UTILS_HPP
#define PEAR_CLI_COMMAND_UTILS_HPP

#include <pear/fs/workspace.hpp>

#include <filesystem>
#include <string>

namespace pear::cli {

std::filesystem::path get_database_path(const pear::storage::Workspace& workspace);
std::string remove_empty_suffix(std::string path);

} // namespace pear::cli

#endif
