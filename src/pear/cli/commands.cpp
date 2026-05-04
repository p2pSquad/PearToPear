#include <pear/cli/commands.hpp>

#include <pear/db/sqlite_database.hpp>
#include <pear/fs/workspace.hpp>
#include <pear/demon/demon.hpp>
#include <pear/net/remote_client.hpp>
#include <pear/fs/hash.hpp>

#include "command_utils.hpp"
#include "output.hpp"
#include "status.hpp"
#include "sync.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <chrono>
#include <thread>
#include <unordered_set>

namespace pear::cli {

void run_init(const std::filesystem::path& workspace_path) {
#ifdef PEAR_DEBUG
    std::cout << "[DEBUG] run_init called\n";
    std::cout << "[DEBUG] workspace_path: " << workspace_path << '\n';
#endif

    pear::storage::Workspace workspace = pear::storage::Workspace::init(workspace_path);
    [[maybe_unused]] pear::db::SqliteDatabase database(get_database_path(workspace));
    std::cout << Grusha << "initialized workspace at " << workspace.get_root() << '\n';
}

void run_deinit() {
#ifdef PEAR_DEBUG
    std::cout << "[DEBUG] run_deinit called\n";
#endif

    namespace fs = std::filesystem;

    pear::storage::Workspace workspace = pear::storage::Workspace::discover();

    if (pear::demon::is_alive(workspace.get_root())) {
        pear::demon::kill(workspace.get_root());

        for (int attempt = 0; attempt < 100; ++attempt) {
            if (!pear::demon::is_alive(workspace.get_root())) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        if (pear::demon::is_alive(workspace.get_root())) {
            throw std::runtime_error("failed to stop demon before deinit");
        }
    }

    for (const auto& entry : fs::directory_iterator(workspace.get_root())) {
        if (entry.is_regular_file() && entry.path().extension() == ".empty") {
            fs::remove(entry.path());
        }
    }

    fs::remove_all(workspace.get_peer_dir());
    std::cout << Grusha << "deinitialized workspace at " << workspace.get_root() << '\n';
}

void run_connect(const std::string& gu_address, const std::string& listen_address, bool is_main) {
#ifdef PEAR_DEBUG
    std::cout << "[DEBUG] run_connect called\n";
    std::cout << "[DEBUG] gu_address: " << gu_address << '\n';
    std::cout << "[DEBUG] listen_address: " << listen_address << '\n';
    std::cout << "[DEBUG] is_main: " << std::boolalpha << is_main << '\n';
#endif

    pear::storage::Workspace workspace = pear::storage::Workspace::discover();
    pear::db::SqliteDatabase database(get_database_path(workspace));

    pear::demon::spawn(workspace.get_root(), listen_address, is_main);

    try {
        if (is_main) {
            uint64_t device_id = database.registerDevice(listen_address);

            database.setMasterAddress(listen_address);
            database.setDeviceId(device_id);

            std::cout << Grusha << "connected as main node at " << listen_address << " device_id=" << device_id << '\n';
            return;
        }

        uint64_t device_id = pear::net::RemoteClient::RegisterDevice(gu_address, listen_address);
        std::vector<pear::net::WalEntryInfo> wal_entries = pear::net::RemoteClient::UpdateDB(gu_address, 0, device_id);

        database.applyWalEntries(wal_entries);
        database.setMasterAddress(gu_address);
        database.setDeviceId(device_id);

        std::cout << Grusha << "connected to " << gu_address << " as device_id=" << device_id << '\n';
    } catch (...) {
        try {
            if (pear::demon::is_alive(workspace.get_root())) {
                pear::demon::kill(workspace.get_root());
            }
        } catch (...) {
        }
        throw;
    }
}

void run_disconnect() {
#ifdef PEAR_DEBUG
    std::cout << "[DEBUG] run_disconnect called\n";
#endif

    pear::storage::Workspace workspace = pear::storage::Workspace::discover();
    pear::db::SqliteDatabase database(get_database_path(workspace));

    bool demon_was_alive = pear::demon::is_alive(workspace.get_root());

    if (demon_was_alive) {
        pear::demon::kill(workspace.get_root());
    }

    database.setMasterAddress("");
    database.setDeviceId(0);

    if (demon_was_alive) {
        std::cout << Grusha << "disconnected\n";
    } else {
        std::cout << Grusha << "already disconnected\n";
    }
}

void run_add(const std::vector<std::filesystem::path>& paths, bool all) {
#ifdef PEAR_DEBUG
    std::cout << "[DEBUG] run_add called\n";
    std::cout << "[DEBUG] all: " << std::boolalpha << all << '\n';

    if (!all) {
        for (const auto& path : paths) {
            std::cout << "[DEBUG] path: " << path << '\n';
        }
    }
#endif

    namespace fs = std::filesystem;

    pear::storage::Workspace workspace = pear::storage::Workspace::discover();
    pear::db::SqliteDatabase database(get_database_path(workspace));

    bool had_errors = false;

    auto stage_file = [&](const fs::path& file_path) {
        try {
            if (file_path.extension() == ".empty") {
                return; 
            }

            const std::string relative_path = workspace.get_relative_path(file_path).generic_string();
            const std::string object_hash = pear::storage::get_file_hash(file_path);
            const auto current_object_hash = database.getObjectHashByPath(relative_path);

            if (current_object_hash && *current_object_hash == object_hash) {
                database.unstageFile(relative_path);
                std::cout << Grusha << "already up to date " << relative_path << '\n';
                return;
            }

            if (!workspace.has_objectfile(object_hash)) {
                workspace.create_objectfile(object_hash, file_path);
            }

            const std::string operation = current_object_hash ? "update" : "add";
            database.stageFile(relative_path, object_hash, file_path.string(), operation);

            std::cout << Grusha << "staged " << relative_path << '\n';
        } catch (const std::exception& error) {
            std::cerr << "error: failed to stage " << file_path << ": " << error.what() << '\n';
            had_errors = true;
        }
    };

    auto stage_path = [&](const fs::path& path) {
        try {
            const auto files = workspace.collect_files(path);

            for (const auto& file_path : files) {
                stage_file(file_path);
            }
        } catch (const std::exception& error) {
            std::cerr << "error: failed to stage " << path << ": " << error.what() << '\n';
            had_errors = true;
        }
    };

    if (all) {
        stage_path(workspace.get_root());
    } else {
        for (const auto& path : paths) {
            stage_path(path);
        }
    }

    if (had_errors) {
        throw std::runtime_error("failed to stage some files");
    }
}

void run_unstage(const std::vector<std::filesystem::path>& paths, bool all) {
#ifdef PEAR_DEBUG
    std::cout << "[DEBUG] run_unstage called\n";
    std::cout << "[DEBUG] all: " << std::boolalpha << all << '\n';

    if (!all) {
        for (const auto& path : paths) {
            std::cout << "[DEBUG] path: " << path << '\n';
        }
    }
#endif

    pear::storage::Workspace workspace = pear::storage::Workspace::discover();
    pear::db::SqliteDatabase database(get_database_path(workspace));

    if (all) {
        database.clearStaging();
        std::cout << Grusha << "cleared staging\n";
        return;
    }

    const auto staged_files = database.getStagedFiles();

    if (staged_files.empty()) {
        std::cout << Grusha << "nothing to unstage\n";
        return;
    }

    bool had_errors = false;
    std::unordered_set<std::string> removed_paths;

    auto should_unstage = [](const std::string& staged_path, const std::string& target_path) {
        if (target_path == ".") {
            return true;
        }

        if (staged_path == target_path) {
            return true;
        }

        const std::string prefix = target_path + "/";
        return staged_path.rfind(prefix, 0) == 0;
    };

    for (const auto& path : paths) {
        try {
            const std::string target_path = workspace.get_relative_path(path).generic_string();

            std::vector<std::string> paths_to_unstage;

            for (const auto& file : staged_files) {
                if (removed_paths.contains(file.path)) {
                    continue;
                }

                if (should_unstage(file.path, target_path)) {
                    paths_to_unstage.push_back(file.path);
                }
            }

            if (paths_to_unstage.empty()) {
                std::cerr << "error: nothing staged under " << target_path << '\n';
                had_errors = true;
                continue;
            }

            std::sort(paths_to_unstage.begin(), paths_to_unstage.end());

            for (const auto& staged_path : paths_to_unstage) {
                database.unstageFile(staged_path);
                removed_paths.insert(staged_path);
                std::cout << Grusha << "unstaged " << staged_path << '\n';
            }
        } catch (const std::exception& error) {
            std::cerr << "error: failed to unstage " << path << ": " << error.what() << '\n';
            had_errors = true;
        }
    }

    if (had_errors) {
        throw std::runtime_error("failed to unstage some files");
    }
}

void run_update() {
#ifdef PEAR_DEBUG
    std::cout << "[DEBUG] run_update called\n";
#endif

    sync_with_master(true);
}

void run_ls() {
#ifdef PEAR_DEBUG
    std::cout << "[DEBUG] run_ls called\n";
#endif

    pear::storage::Workspace workspace = pear::storage::Workspace::discover();
    pear::db::SqliteDatabase database(get_database_path(workspace));

    const auto files = database.getAllFiles();
    print_file_tree(files, database);
}

void run_push() {
#ifdef PEAR_DEBUG
    std::cout << "[DEBUG] run_push called\n";
#endif

    pear::storage::Workspace workspace = pear::storage::Workspace::discover();

    {
        pear::db::SqliteDatabase database(get_database_path(workspace));
        const std::string master_address = database.getMasterAddress();
        const uint64_t device_id = database.getDeviceId();

        if (master_address.empty()) {
            throw std::runtime_error("not connected: master address is empty");
        }
        if (device_id == 0) {
            throw std::runtime_error("not connected: device id is unknown");
        }
    }

    sync_with_master(false);

    pear::db::SqliteDatabase database(get_database_path(workspace));
    const std::string master_address = database.getMasterAddress();
    const uint64_t device_id = database.getDeviceId();
    const auto staged_files = database.getStagedFiles();

    if (staged_files.empty()) {
        std::cout << Grusha << "nothing to push\n";
        return;
    }

    bool had_errors = false;
    std::vector<pear::net::WalEntryInfo> wal_entries;
    std::vector<std::string> pushed_paths;

    for (const auto& file : staged_files) {
        try {
            if (file.operation != "add" && file.operation != "update") {
                throw std::runtime_error("unsupported staged operation: " + file.operation);
            }

            if (!workspace.has_objectfile(file.object_hash)) {
                throw std::runtime_error("staged object does not exist: " + file.object_hash);
            }

            const uint64_t version = database.getNextVersion(file.path);

            pear::net::WalEntryInfo entry {};
            entry.op_type = pear::net::WalOpTypeInfo::kFileUpdate;
            entry.file.path = file.path;
            entry.file.object_hash = file.object_hash;
            entry.file.version = version;
            entry.file.owner_device_id = device_id;

            wal_entries.push_back(entry);
            pushed_paths.push_back(file.path);
        } catch (const std::exception& error) {
            std::cerr << "error: failed to prepare push for " << file.path << ": " << error.what() << '\n';
            had_errors = true;
        }
    }

    if (wal_entries.empty()) {
        throw std::runtime_error("failed to prepare push");
    }

    std::vector<uint64_t> assigned_seq_ids;
    if (!pear::net::RemoteClient::PushWAL(master_address, device_id, wal_entries, assigned_seq_ids)) {
        throw std::runtime_error("failed to push wal to main node");
    }

    for (const auto& path : pushed_paths) {
        database.unstageFile(path);
    }

    sync_with_master(false);

    for (const auto& path : pushed_paths) {
        std::cout << Grusha << "pushed " << path << '\n';
    }

    if (had_errors) {
        throw std::runtime_error("failed to push some staged files");
    }
}

void run_pull(const std::vector<std::string>& targets) {
#ifdef PEAR_DEBUG
    std::cout << "[DEBUG] run_pull called\n";

    for (const auto& target : targets) {
        std::cout << "[DEBUG] target: " << target << '\n';
    }
#endif

    namespace fs = std::filesystem;

    pear::storage::Workspace workspace = pear::storage::Workspace::discover();

    {
        pear::db::SqliteDatabase database(get_database_path(workspace));

        const std::string master_address = database.getMasterAddress();
        const uint64_t device_id = database.getDeviceId();

        if (master_address.empty()) {
            throw std::runtime_error("not connected: master address is empty");
        }

        if (device_id == 0) {
            throw std::runtime_error("not connected: device id is unknown");
        }
    }

    sync_with_master(false);

    pear::db::SqliteDatabase database(get_database_path(workspace));

    const uint64_t device_id = database.getDeviceId();
    const auto all_files = database.getAllFiles();

    bool had_errors = false;

    auto pull_file = [&](const pear::net::FileUpdateInfo& file) {
        if (file.object_hash.empty()) {
            throw std::runtime_error("file has empty object hash");
        }

        const fs::path destination_path = workspace.get_root() / file.path;
        fs::create_directories(destination_path.parent_path());

        if (workspace.has_objectfile(file.object_hash)) {
            fs::copy_file(workspace.get_objectfile_path(file.object_hash), destination_path, fs::copy_options::overwrite_existing);
        } else {
            const std::string owner_address = database.getDeviceAddress(file.owner_device_id);

            if (owner_address.empty()) {
                throw std::runtime_error("owner address is unknown");
            }

            pear::net::RemoteClient::DownloadFile(owner_address, file.object_hash, device_id, destination_path.string());
        }

        const fs::path empty_path = workspace.get_root() / (file.path + ".empty");
        if (fs::exists(empty_path)) {
            fs::remove(empty_path);
        }

        std::cout << Grusha << "pulled " << file.path << '\n';
    };

    for (const auto& target : targets) {
        try {
            const std::string cleaned_target = remove_empty_suffix(target);
            const std::string target_path = workspace.get_relative_path(fs::path(cleaned_target)).generic_string();

            std::vector<pear::net::FileUpdateInfo> files_to_pull;

            auto file_info = database.getFileInfoByPath(target_path, 0);
            if (file_info) {
                files_to_pull.push_back(*file_info);
            } else {
                std::string prefix = target_path;

                while (!prefix.empty() && prefix.back() == '/') {
                    prefix.pop_back();
                }

                prefix += '/';

                for (const auto& file : all_files) {
                    if (file.path.rfind(prefix, 0) == 0) {
                        files_to_pull.push_back(file);
                    }
                }
            }

            if (files_to_pull.empty()) {
                throw std::runtime_error("file or directory not found");
            }

            std::sort(
                files_to_pull.begin(),
                files_to_pull.end(),
                [](const auto& lhs, const auto& rhs) {
                    return lhs.path < rhs.path;
                }
            );

            for (const auto& file : files_to_pull) {
                pull_file(file);
            }
        } catch (const std::exception& error) {
            std::cerr << "error: failed to pull " << target << ": " << error.what() << '\n';
            had_errors = true;
        }
    }

    if (had_errors) {
        throw std::runtime_error("failed to pull some files");
    }
}

void run_status() {
#ifdef PEAR_DEBUG
    std::cout << "[DEBUG] run_status called\n";
#endif

    pear::storage::Workspace workspace = pear::storage::Workspace::discover();
    pear::db::SqliteDatabase database(get_database_path(workspace));

    const StatusInfo status = collect_status_info(workspace, database);
    print_status_info(status);
}

} // namespace pear::cli