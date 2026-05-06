#include "json_output.hpp"

#include <iostream>

#include <nlohmann/json.hpp>

namespace pear::cli {

namespace {

using json = nlohmann::json;

json staged_file_to_json(const pear::db::StagedFileInfo& file) {
    return {
        {"path", file.path},
        {"operation", file.operation},
        {"object_hash", file.object_hash}
    };
}

json file_to_json(const pear::net::FileUpdateInfo& file, pear::db::SqliteDatabase& database) {
    const std::string owner_address = database.getDeviceAddress(file.owner_device_id);

    return {
        {"path", file.path},
        {"object_hash", file.object_hash},
        {"version", file.version},
        {"owner_device_id", file.owner_device_id},
        {"owner_address", owner_address}
    };
}

} // namespace

void print_status_json(const StatusInfo& status) {
    json result;

    result["master_address"] = status.master_address;
    result["device_id"] = status.device_id;

    result["staged"] = json::array();
    for (const auto& file : status.staged_files) {
        result["staged"].push_back(staged_file_to_json(file));
    }

    result["modified"] = status.modified_paths;
    result["modified_after_staging"] = status.modified_after_staging_paths;
    result["missing"] = status.missing_paths;
    result["untracked"] = status.untracked_entries;

    std::cout << result.dump(2) << '\n';
}

void print_ls_json(const std::vector<pear::net::FileUpdateInfo>& files, pear::db::SqliteDatabase& database) {
    json result;
    result["files"] = json::array();

    for (const auto& file : files) {
        result["files"].push_back(file_to_json(file, database));
    }

    std::cout << result.dump(2) << '\n';
}

} // namespace pear::cli
