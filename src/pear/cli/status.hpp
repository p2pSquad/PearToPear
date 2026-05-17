#ifndef PEAR_CLI_STATUS_HPP
#define PEAR_CLI_STATUS_HPP

#include <pear/db/sqlite_database.hpp>
#include <pear/fs/workspace.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace pear::cli {

struct StatusInfo {
    std::string master_address;
    uint64_t device_id = 0;

    std::vector<pear::db::StagedFileInfo> staged_files;
    std::vector<std::string> staged_paths;
    std::vector<std::string> modified_paths;
    std::vector<std::string> modified_after_staging_paths;
    std::vector<std::string> missing_paths;
    std::vector<std::string> untracked_entries;
};

StatusInfo collect_status_info(const pear::storage::Workspace& workspace, pear::db::SqliteDatabase& database);

} // namespace pear::cli

#endif
