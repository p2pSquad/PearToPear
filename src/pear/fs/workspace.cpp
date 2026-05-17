#include <pear/fs/workspace.hpp>

#include <fstream>
#include <stdexcept>
#include <algorithm>

namespace {
namespace fs = std::filesystem;
fs::path normalize_path(const fs::path& path) {
    std::error_code error;
    fs::path absolute_path = fs::absolute(path, error);
    if (error) {
        throw std::runtime_error("Failed to make path absolute");
    }
    fs::path normalized_path = fs::weakly_canonical(absolute_path, error);
    if (error) {
        return absolute_path.lexically_normal();
    }
    return normalized_path;
}

std::optional<fs::path> find_peer_root(const fs::path& start_dir) {
    fs::path current_dir = normalize_path(start_dir);
    while (!fs::exists(current_dir / ".peer")) {
        if (current_dir == current_dir.parent_path()) {
            return std::nullopt;
        }
        current_dir = current_dir.parent_path();
    }
    return current_dir;
}

bool is_path_in_peer_dir(const fs::path& relative_path) {
    auto it = relative_path.begin();
    return it != relative_path.end() && *it == ".peer";
}

} // anonymous namespace

namespace pear::storage {

Workspace::Workspace(fs::path root)
    : m_root(normalize_path(root)),
      m_peer_dir(m_root / ".peer"),
      m_obj_dir(m_peer_dir / "obj"),
      m_meta_dir(m_peer_dir / "meta") {}

fs::path Workspace::create_empty_file(const std::string& relative_path) {
    fs::path file_path = m_root / (relative_path + ".empty");

    if (fs::exists(file_path)) {
        return file_path;
    }

    fs::create_directories(file_path.parent_path());

    std::ofstream file(file_path);
    if (!file) {
        throw std::runtime_error("Failed to create empty file: " + file_path.string());
    }

    file.close();

    fs::permissions(
        file_path,
        fs::perms::owner_read | fs::perms::group_read | fs::perms::others_read,
        fs::perm_options::replace
    );
    return file_path;
}

// getters:
const fs::path& Workspace::get_root() const {
    return m_root;
}
const fs::path& Workspace::get_peer_dir() const {
    return m_peer_dir;
}
const fs::path& Workspace::get_obj_dir() const {
    return m_obj_dir;
}
const fs::path& Workspace::get_meta_dir() const {
    return m_meta_dir;
}

Workspace Workspace::init(const fs::path& root) {
    if (find_peer_root(root))
        throw std::runtime_error("Workspace already initialized");
    Workspace tmp_workspace(root);
    fs::create_directories(tmp_workspace.get_peer_dir());
    fs::create_directories(tmp_workspace.get_obj_dir());
    fs::create_directories(tmp_workspace.get_meta_dir());
    return tmp_workspace;
}

Workspace Workspace::discover(const fs::path& start_dir) {
    auto root = find_peer_root(start_dir);
    if (!root)
        throw std::runtime_error("No workspace found");
    return Workspace(*root);
}

fs::path Workspace::create_objectfile(const std::string& object_name, const fs::path& path_to_source_file) {
    if (!fs::exists(path_to_source_file) || !fs::is_regular_file(path_to_source_file)) {
        throw std::runtime_error("Invalid file");
    }
    fs::path object_path = m_obj_dir / object_name;
    fs::copy_file(path_to_source_file, object_path, fs::copy_options::overwrite_existing);
    return object_path;
}

fs::path Workspace::get_objectfile_path(const std::string& object_name) const {
    fs::path object_path = m_obj_dir / object_name;
    if (!fs::exists(object_path) || !fs::is_regular_file(object_path)) {
        throw std::runtime_error("Invalid object file");
    }
    return object_path;
}

void Workspace::delete_objectfile(const std::string& object_name) {
    fs::path object_path = m_obj_dir / object_name;
    if (!fs::exists(object_path) || !fs::is_regular_file(object_path))
        throw std::runtime_error("Invalid object file");
    fs::remove(object_path);
}

void Workspace::create_all_empty_files(const std::vector<std::string>& names_to_meta_files) {
    for (const auto& path : names_to_meta_files) {
        create_empty_file(path);
    }
}

std::vector<std::string> Workspace::get_list_object_ids() const {
    std::vector<std::string> object_ids;
    for (const auto& entry : fs::directory_iterator(m_obj_dir)) {
        if (entry.is_regular_file()) {
            object_ids.push_back(entry.path().filename().string());
        }
    }
    return object_ids;
}

bool Workspace::has_objectfile(const std::string& object_name) const {
    fs::path object_path = m_obj_dir / object_name;
    return fs::is_regular_file(object_path);
}

std::filesystem::path Workspace::get_relative_path(const std::filesystem::path& path) const {
    fs::path absolute_path = normalize_path(path);
    fs::path relative_path = fs::relative(absolute_path, m_root);

    auto it = relative_path.begin();
    if (relative_path.empty() || it == relative_path.end() || *it == "..") {
        throw std::runtime_error("path is outside workspace at " + m_root.string());
    }

    if (is_path_in_peer_dir(relative_path)) {
        throw std::runtime_error("path points to service directory");
    }

    return relative_path;
}

std::vector<std::filesystem::path> Workspace::collect_files(const std::filesystem::path& path) const {
    std::vector<std::filesystem::path> files;
    fs::path absolute_path = normalize_path(path);

    get_relative_path(absolute_path);

    if (!fs::exists(absolute_path)) {
        throw std::runtime_error("path does not exist");
    }

    if (fs::is_regular_file(absolute_path)) {
        files.push_back(absolute_path);
    } else if (fs::is_directory(absolute_path)) {
        fs::recursive_directory_iterator iterator(absolute_path);
        fs::recursive_directory_iterator end;
        while (iterator != end) {
            fs::path entry_path = normalize_path(iterator->path());

            if (entry_path == m_peer_dir) {
                iterator.disable_recursion_pending();
                ++iterator;
                continue;
            }

            if (iterator->is_regular_file()) {
                files.push_back(entry_path);
            }

            ++iterator;
        }
    } else {
        throw std::runtime_error("path is not a regular file or directory");
    }

    std::sort(files.begin(), files.end());
    return files;
}

}  // namespace pear::storage