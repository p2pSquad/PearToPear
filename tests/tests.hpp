#ifndef PEAR_TESTS_HPP
#define PEAR_TESTS_HPP

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace pear::tests {

namespace fs = std::filesystem;
using json = nlohmann::json;

inline fs::path pear_binary;

struct CommandResult {
    int code = -1;
    std::string out;
    std::string err;
};

struct StatusEntry {
    std::string path;
    std::string operation;
    std::string object_hash;
};

struct Status {
    std::vector<StatusEntry> staged;
    std::vector<std::string> modified;
    std::vector<std::string> modified_after_staging;
    std::vector<std::string> missing;
    std::vector<std::string> untracked;
};

struct FileEntry {
    std::string path;
    std::string object_hash;
    uint64_t version = 0;
    uint64_t owner_device_id = 0;
    std::string owner_address;
    std::vector<uint64_t> object_owner_device_ids;
};

struct Ls {
    std::vector<FileEntry> files;
};

inline std::string shell_quote(const std::string& value) {
    std::string result = "'";

    for (char character : value) {
        if (character == '\'') {
            result += "'\\''";
        } else {
            result += character;
        }
    }

    result += "'";
    return result;
}

inline std::string read_file(const fs::path& path) {
    std::ifstream input(path);
    std::stringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

inline fs::path temp_path() {
    const auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    return fs::temp_directory_path() / ("pear_test_" + std::to_string(now));
}

inline std::string local_address() {
    static int port = 25000 + static_cast<int>(std::chrono::high_resolution_clock::now().time_since_epoch().count() % 10000);
    return "127.0.0.1:" + std::to_string(port++);
}

inline void wait_network() {
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
}

inline std::vector<std::string> staged_paths(const Status& status) {
    std::vector<std::string> result;

    for (const auto& entry : status.staged) {
        result.push_back(entry.path);
    }

    return result;
}

inline std::vector<std::string> file_paths(const Ls& ls) {
    std::vector<std::string> result;

    for (const auto& file : ls.files) {
        result.push_back(file.path);
    }

    return result;
}

class Repo {
public:
    Repo()
        : temp_dir_(temp_path()),
          root_(temp_dir_ / "repo") {
        fs::create_directories(temp_dir_);
    }

    ~Repo() {
        if (fs::exists(root_)) {
            run(root_, {"disconnect"});
            run(root_, {"deinit"});
        }

        std::error_code error;
        fs::remove_all(temp_dir_, error);
    }

    CommandResult init() const {
        return run(temp_dir_, {"init", root_.string()});
    }

    CommandResult deinit() const {
        return run(root_, {"deinit"});
    }

    CommandResult connect_main(const std::string& listen_address) const {
        return run(root_, {"connect", "--main", "--listen", listen_address});
    }

    CommandResult connect(const std::string& main_address, const std::string& listen_address) const {
        return run(root_, {"connect", "--gu", main_address, "--listen", listen_address});
    }

    CommandResult disconnect() const {
        return run(root_, {"disconnect"});
    }

    CommandResult add(const std::vector<std::string>& paths) const {
        std::vector<std::string> args = {"add"};
        args.insert(args.end(), paths.begin(), paths.end());
        return run(root_, args);
    }

    CommandResult add_all() const {
        return run(root_, {"add", "--all"});
    }

    CommandResult unstage(const std::vector<std::string>& paths) const {
        std::vector<std::string> args = {"unstage"};
        args.insert(args.end(), paths.begin(), paths.end());
        return run(root_, args);
    }

    CommandResult unstage_all() const {
        return run(root_, {"unstage", "--all"});
    }

    CommandResult update() const {
        return run(root_, {"update"});
    }

    CommandResult push() const {
        return run(root_, {"push"});
    }

    CommandResult pull(const std::vector<std::string>& targets) const {
        std::vector<std::string> args = {"pull"};
        args.insert(args.end(), targets.begin(), targets.end());
        return run(root_, args);
    }

    CommandResult raw(const std::vector<std::string>& args) const {
        return run(root_, args);
    }

    CommandResult log() const {
        return run(root_, {"log"});
    }

    CommandResult log_tail(size_t line_count) const {
        return run(root_, {"log", "--tail", std::to_string(line_count)});
    }

    Status status() const {
        const CommandResult result = run(root_, {"status", "--json"});
        EXPECT_EQ(result.code, 0) << result.out << result.err;

        const json data = json::parse(result.out, nullptr, false);
        EXPECT_FALSE(data.is_discarded()) << result.out << result.err;

        Status status;

        if (data.is_discarded()) {
            return status;
        }

        for (const auto& item : data.at("staged")) {
            status.staged.push_back({
                item.at("path").get<std::string>(),
                item.at("operation").get<std::string>(),
                item.at("object_hash").get<std::string>()
            });
        }

        status.modified = data.at("modified").get<std::vector<std::string>>();
        status.modified_after_staging = data.at("modified_after_staging").get<std::vector<std::string>>();
        status.missing = data.at("missing").get<std::vector<std::string>>();
        status.untracked = data.at("untracked").get<std::vector<std::string>>();

        return status;
    }

    Ls ls() const {
        const CommandResult result = run(root_, {"ls", "--json"});
        EXPECT_EQ(result.code, 0) << result.out << result.err;

        const json data = json::parse(result.out, nullptr, false);
        EXPECT_FALSE(data.is_discarded()) << result.out << result.err;

        Ls ls;

        if (data.is_discarded()) {
            return ls;
        }

        for (const auto& item : data.at("files")) {
            ls.files.push_back({
                item.at("path").get<std::string>(),
                item.at("object_hash").get<std::string>(),
                item.at("version").get<uint64_t>(),
                item.at("owner_device_id").get<uint64_t>(),
                item.at("owner_address").get<std::string>(),
                item.value("object_owner_device_ids", std::vector<uint64_t>{})
            });
        }

        return ls;
    }

    void write_file(const fs::path& path, std::string_view content) const {
        const fs::path full_path = root_ / path;
        fs::create_directories(full_path.parent_path());

        std::ofstream output(full_path);
        output << content;
    }

    std::string read_file(const fs::path& path) const {
        return pear::tests::read_file(root_ / path);
    }

    void remove_file(const fs::path& path) const {
        fs::remove(root_ / path);
    }

    bool exists(const fs::path& path) const {
        return fs::exists(root_ / path);
    }

    const fs::path& root() const {
        return root_;
    }

private:
    CommandResult run(const fs::path& working_directory, const std::vector<std::string>& args) const {
        const fs::path out_path = temp_dir_ / "out.txt";
        const fs::path err_path = temp_dir_ / "err.txt";

        std::string command = "cd " + shell_quote(working_directory.string()) + " && " + shell_quote(pear_binary.string());

        for (const std::string& arg : args) {
            command += " " + shell_quote(arg);
        }

        command += " > " + shell_quote(out_path.string()) + " 2> " + shell_quote(err_path.string());

        CommandResult result;
        result.code = std::system(command.c_str());
        result.out = pear::tests::read_file(out_path);
        result.err = pear::tests::read_file(err_path);

        return result;
    }

    fs::path temp_dir_;
    fs::path root_;
};

} // namespace pear::tests

#endif // PEAR_TESTS_HPP
