#include "status.hpp"

#include <pear/fs/hash.hpp>
#include <pear/net/db_types.hpp>

#include <algorithm>
#include <filesystem>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace pear::cli {

std::unordered_map<std::string, pear::db::StagedFileInfo> make_staged_map(const std::vector<pear::db::StagedFileInfo>& staged_files) {
    std::unordered_map<std::string, pear::db::StagedFileInfo> staged_by_path;

    for (const auto& file : staged_files) {
        staged_by_path[file.path] = file;
    }

    return staged_by_path;
}

std::unordered_map<std::string, pear::net::FileUpdateInfo> make_tracked_map(const std::vector<pear::net::FileUpdateInfo>& tracked_files) {
    std::unordered_map<std::string, pear::net::FileUpdateInfo> tracked_by_path;

    for (const auto& file : tracked_files) {
        tracked_by_path[file.path] = file;
    }

    return tracked_by_path;
}

std::unordered_map<std::string, std::string> collect_local_hashes(const pear::storage::Workspace& workspace) {
    std::unordered_map<std::string, std::string> local_hash_by_path;

    for (const auto& file_path : workspace.collect_files(workspace.get_root())) {
        if (file_path.extension() == ".empty") {
            continue;
        }

        const std::string relative_path = workspace.get_relative_path(file_path).generic_string();
        local_hash_by_path[relative_path] = pear::storage::get_file_hash(file_path);
    }

    return local_hash_by_path;
}

std::vector<std::string> find_untracked_paths(const std::unordered_map<std::string, std::string>& local_hash_by_path, const std::unordered_map<std::string, pear::db::StagedFileInfo>& staged_by_path, const std::unordered_map<std::string, pear::net::FileUpdateInfo>& tracked_by_path) {
    std::vector<std::string> untracked_paths;

    for (const auto& item : local_hash_by_path) {
        const std::string& path = item.first;

        if (staged_by_path.find(path) == staged_by_path.end() && tracked_by_path.find(path) == tracked_by_path.end()) {
            untracked_paths.push_back(path);
        }
    }

    std::sort(untracked_paths.begin(), untracked_paths.end());
    return untracked_paths;
}

bool is_fully_untracked_directory(const pear::storage::Workspace& workspace, const std::filesystem::path& relative_dir, const std::unordered_set<std::string>& untracked_path_set) {
    namespace fs = std::filesystem;

    const fs::path directory_path = workspace.get_root() / relative_dir;

    if (!fs::exists(directory_path) || !fs::is_directory(directory_path)) {
        return false;
    }

    bool has_user_files = false;

    for (const auto& file_path : workspace.collect_files(directory_path)) {
        if (file_path.extension() == ".empty") {
            continue;
        }

        has_user_files = true;

        const std::string relative_path = workspace.get_relative_path(file_path).generic_string();
        if (untracked_path_set.find(relative_path) == untracked_path_set.end()) {
            return false;
        }
    }

    return has_user_files;
}

std::vector<std::string> compress_untracked_paths(const pear::storage::Workspace& workspace, const std::vector<std::string>& untracked_paths) {
    namespace fs = std::filesystem;

    std::set<std::string> compressed_entries;
    std::unordered_set<std::string> untracked_path_set(untracked_paths.begin(), untracked_paths.end());

    for (const auto& path : untracked_paths) {
        fs::path best_directory;
        fs::path parent = fs::path(path).parent_path();

        while (!parent.empty()) {
            if (is_fully_untracked_directory(workspace, parent, untracked_path_set)) {
                best_directory = parent;
            }

            parent = parent.parent_path();
        }

        if (best_directory.empty()) {
            compressed_entries.insert(path);
        } else {
            compressed_entries.insert(best_directory.generic_string() + "/");
        }
    }

    return std::vector<std::string>(compressed_entries.begin(), compressed_entries.end());
}

StatusInfo collect_status_info(const pear::storage::Workspace& workspace, pear::db::SqliteDatabase& database) {
    namespace fs = std::filesystem;

    StatusInfo status;
    status.master_address = database.getMasterAddress();
    status.device_id = database.getDeviceId();
    status.staged_files = database.getStagedFiles();

    const auto tracked_files = database.getAllFiles();
    const auto staged_by_path = make_staged_map(status.staged_files);
    const auto tracked_by_path = make_tracked_map(tracked_files);
    const auto local_hash_by_path = collect_local_hashes(workspace);

    for (const auto& file : status.staged_files) {
        status.staged_paths.push_back(file.path);

        auto local_hash = local_hash_by_path.find(file.path);
        if (local_hash == local_hash_by_path.end()) {
            status.missing_paths.push_back(file.path);
        } else if (local_hash->second != file.object_hash) {
            status.modified_after_staging_paths.push_back(file.path);
        }
    }

    for (const auto& file : tracked_files) {
        if (staged_by_path.find(file.path) != staged_by_path.end()) {
            continue;
        }

        auto local_hash = local_hash_by_path.find(file.path);
        if (local_hash != local_hash_by_path.end() && local_hash->second != file.object_hash) {
            status.modified_paths.push_back(file.path);
            continue;
        }

        const fs::path empty_path = workspace.get_root() / (file.path + ".empty");
        if (local_hash == local_hash_by_path.end() && !fs::exists(empty_path)) {
            status.missing_paths.push_back(file.path);
        }
    }

    const auto untracked_paths = find_untracked_paths(local_hash_by_path, staged_by_path, tracked_by_path);
    status.untracked_entries = compress_untracked_paths(workspace, untracked_paths);

    std::sort(status.staged_files.begin(), status.staged_files.end(), [](const auto& lhs, const auto& rhs) { return lhs.path < rhs.path; });
    std::sort(status.staged_paths.begin(), status.staged_paths.end());
    std::sort(status.modified_paths.begin(), status.modified_paths.end());
    std::sort(status.modified_after_staging_paths.begin(), status.modified_after_staging_paths.end());
    std::sort(status.missing_paths.begin(), status.missing_paths.end());

    return status;
}

} // namespace pear::cli
