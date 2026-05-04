#include "command_utils.hpp"

namespace pear::cli {

std::filesystem::path get_database_path(const pear::storage::Workspace& workspace) {
    return workspace.get_meta_dir() / "peer.db";
}

std::string remove_empty_suffix(std::string path) {
    const std::string empty_suffix = ".empty";

    if (path.size() >= empty_suffix.size() && path.compare(path.size() - empty_suffix.size(), empty_suffix.size(), empty_suffix) == 0) {
        path.erase(path.size() - empty_suffix.size());
    }

    return path;
}

} // namespace pear::cli
