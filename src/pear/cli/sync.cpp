#include "sync.hpp"

#include "command_utils.hpp"
#include "output.hpp"

#include <pear/db/sqlite_database.hpp>
#include <pear/fs/workspace.hpp>
#include <pear/net/remote_client.hpp>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace pear::cli {

void sync_with_master(bool verbose) {
    namespace fs = std::filesystem;

    pear::storage::Workspace workspace = pear::storage::Workspace::discover();
    pear::db::SqliteDatabase database(get_database_path(workspace));

    const std::string master_address = database.getMasterAddress();
    const uint64_t device_id = database.getDeviceId();

    if (master_address.empty()) {
        throw std::runtime_error("not connected: master address is empty");
    }
    if (device_id == 0) {
        throw std::runtime_error("not connected: device id is unknown");
    }

    const uint64_t last_seq_id = database.getLastSeqId();
    const auto wal_entries = pear::net::RemoteClient::UpdateDB(master_address, last_seq_id, device_id);

    if (!wal_entries.empty()) {
        database.applyWalEntries(wal_entries);
    }

    const auto tracked_files = database.getAllFiles();
    std::unordered_set<std::string> desired_empty_paths;

    for (const auto& file : tracked_files) {
        const fs::path workspace_file_path = workspace.get_root() / file.path;

        if (!fs::exists(workspace_file_path)) {
            desired_empty_paths.insert(file.path);
        }
    }

    fs::recursive_directory_iterator iterator(workspace.get_root());
    fs::recursive_directory_iterator end;

    while (iterator != end) {
        if (iterator->path() == workspace.get_peer_dir()) {
            iterator.disable_recursion_pending();
            ++iterator;
            continue;
        }

        if (!iterator->is_regular_file() || iterator->path().extension() != ".empty") {
            ++iterator;
            continue;
        }

        const std::string empty_relative_path = workspace.get_relative_path(iterator->path()).generic_string();
        const std::string file_relative_path = empty_relative_path.substr(0, empty_relative_path.size() - std::string(".empty").size());

        if (!desired_empty_paths.contains(file_relative_path)) {
            fs::remove(iterator->path());
        }

        ++iterator;
    }

    std::vector<std::string> empty_paths(desired_empty_paths.begin(), desired_empty_paths.end());
    std::sort(empty_paths.begin(), empty_paths.end());

    workspace.create_all_empty_files(empty_paths);

    if (!verbose) {
        return;
    }

    if (wal_entries.empty()) {
        std::cout << Grusha << "already up to date\n";
    } else {
        std::cout << Grusha << "applied " << wal_entries.size() << " wal entries\n";
    }
}

} // namespace pear::cli
