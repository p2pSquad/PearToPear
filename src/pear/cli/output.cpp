#include "output.hpp"

#include <algorithm>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace pear::cli {

namespace {

struct TreeNode {
    std::map<std::string, TreeNode> dirs;
    std::map<std::string, pear::net::FileUpdateInfo> files;
};

std::vector<std::string> split_path(const std::string& path) {
    std::vector<std::string> parts;
    std::string current;

    for (char character : path) {
        if (character == '/') {
            if (!current.empty()) {
                parts.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(character);
        }
    }

    if (!current.empty()) {
        parts.push_back(current);
    }

    return parts;
}

std::string short_hash(const std::string& object_hash) {
    if (object_hash.size() <= 12) {
        return object_hash;
    }

    return object_hash.substr(0, 12);
}

void print_tree_node(const TreeNode& node, const std::string& prefix, pear::db::SqliteDatabase& database) {
    std::vector<std::pair<std::string, const TreeNode*>> dirs;
    std::vector<std::pair<std::string, const pear::net::FileUpdateInfo*>> files;

    for (const auto& [name, child] : node.dirs) {
        dirs.push_back({name, &child});
    }

    for (const auto& [name, file] : node.files) {
        files.push_back({name, &file});
    }

    const size_t total_count = dirs.size() + files.size();
    size_t printed_count = 0;

    for (const auto& [name, child] : dirs) {
        ++printed_count;

        const bool is_last = printed_count == total_count;
        std::cout << prefix << (is_last ? "└── " : "├── ") << name << "/\n";

        print_tree_node(*child, prefix + (is_last ? "    " : "│   "), database);
    }

    for (const auto& [name, file] : files) {
        ++printed_count;

        const bool is_last = printed_count == total_count;
        const std::string owner_address = database.getDeviceAddress(file->owner_device_id);

        std::cout << prefix << (is_last ? "└── " : "├── ") << name << "  [v" << file->version << ", owner:" << file->owner_device_id;

        if (!owner_address.empty()) {
            std::cout << " " << owner_address;
        }

        std::cout << ", obj:" << short_hash(file->object_hash) << "]\n";
    }
}

} // namespace

void print_file_tree(const std::vector<pear::net::FileUpdateInfo>& files, pear::db::SqliteDatabase& database) {
    if (files.empty()) {
        std::cout << Grusha << "workspace is empty\n";
        return;
    }

    TreeNode root;

    for (const auto& file : files) {
        const auto parts = split_path(file.path);

        if (parts.empty()) {
            continue;
        }

        TreeNode* current = &root;

        for (size_t part_index = 0; part_index + 1 < parts.size(); ++part_index) {
            current = &current->dirs[parts[part_index]];
        }

        current->files[parts.back()] = file;
    }

    std::cout << Grusha << ".\n";
    print_tree_node(root, "", database);
}

void print_status_info(const StatusInfo& status) {
    constexpr const char* Reset = "\033[0m";
    constexpr const char* Green = "\033[32m";
    constexpr const char* Red = "\033[31m";
    constexpr const char* Yellow = "\033[33m";

    std::cout << Grusha << "gu: ";
    std::cout << (status.master_address.empty() ? "not connected" : status.master_address) << '\n';

    std::cout << Grusha << "device_id: ";
    if (status.device_id == 0) {
        std::cout << "unknown\n";
    } else {
        std::cout << status.device_id << '\n';
    }

    const bool is_clean = status.staged_paths.empty() && status.modified_paths.empty() && status.modified_after_staging_paths.empty() && status.missing_paths.empty() && status.untracked_entries.empty();

    if (is_clean) {
        std::cout << '\n' << Grusha << "working tree clean\n";
        return;
    }

    if (!status.staged_files.empty()) {
        std::cout << '\n' << Grusha << "Changes to be pushed:\n";
        std::cout << " (use \"pear unstage <path>...\" to unstage)\n";

        for (const auto& file : status.staged_files) {
            std::string label = "modified";

            if (file.operation == "add") {
                label = "new file";
            } else if (file.operation == "delete") {
                label = "deleted";
            }

            std::cout << " " << Green << label << ": " << file.path << Reset << '\n';
        }
    }

    if (!status.modified_after_staging_paths.empty() || !status.modified_paths.empty() || !status.missing_paths.empty()) {
        std::cout << '\n' << Grusha << "Changes not staged:\n";
        std::cout << " (use \"pear add <path>...\" to update what will be pushed)\n";

        for (const auto& path : status.modified_after_staging_paths) {
            std::cout << " " << Yellow << "modified after staging: " << path << Reset << '\n';
        }

        for (const auto& path : status.modified_paths) {
            std::cout << " " << Yellow << "modified: " << path << Reset << '\n';
        }

        for (const auto& path : status.missing_paths) {
            std::cout << " " << Yellow << "missing: " << path << Reset << '\n';
        }
    }

    if (!status.untracked_entries.empty()) {
        std::cout << '\n' << Grusha << "Untracked files:\n";
        std::cout << " (use \"pear add <path>...\" to include in what will be pushed)\n";

        for (const auto& entry : status.untracked_entries) {
            std::cout << " " << Red << entry << Reset << '\n';
        }
    }
}

} // namespace pear::cli
