#ifndef PEAR_FILESYSTEM_WORKSPACE_HPP
#define PEAR_FILESYSTEM_WORKSPACE_HPP

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace pear::storage {

namespace fs = std::filesystem;

struct Workspace {
private:
    fs::path m_root;
    fs::path m_peer_dir;
    fs::path m_obj_dir;
    fs::path m_meta_dir;

    explicit Workspace(fs::path root);

    fs::path create_empty_file(const std::string& filename);

public:

    // getters:
    const fs::path& get_root() const;
    const fs::path& get_peer_dir() const;
    const fs::path& get_obj_dir() const;
    const fs::path& get_meta_dir() const;

    static Workspace init(const fs::path& root = fs::current_path());
    static Workspace discover(const fs::path& start_dir = fs::current_path());

    fs::path create_objectfile(const std::string& object_name, const fs::path& path_to_source_file);
    fs::path get_objectfile_path(const std::string& object_name) const;
    void delete_objectfile(const std::string& object_name);

    void create_all_empty_files(const std::vector<std::string>& names_to_meta_files);

    std::vector<std::string> get_list_object_ids() const;

    bool has_objectfile(const std::string& object_name) const;

    std::filesystem::path get_relative_path(const std::filesystem::path& path) const;
    std::vector<std::filesystem::path> collect_files(const std::filesystem::path& path) const;

};

}  // namespace pear::storage

#endif  // PEAR_FILESYSTEM_WORKSPACE_HPP