#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>

#include <pear/cli/commands.hpp>

int main(int argc, char** argv) {
    CLI::App app{"pear"};

    std::filesystem::path workspace_path = ".";
    CLI::App* init = app.add_subcommand("init", "Initialize a new Pear workspace");
    init->add_option("path", workspace_path, "Workspace directory");

    CLI::App* deinit = app.add_subcommand("deinit", "Remove the local Pear workspace");

    std::string gu_address;
    std::string listen_address;
    bool is_main = false;

    CLI::App* connect = app.add_subcommand("connect", "Connect to a Pear workspace");
    auto* connect_main_opt = connect->add_flag("--main", is_main, "Run as main node");
    auto* connect_gu_opt = connect->add_option("--gu", gu_address, "Main node address ip:port");
    auto* connect_listen_opt = connect->add_option("--listen", listen_address, "Local listen address ip:port")->required();

    connect_main_opt->excludes(connect_gu_opt);

    CLI::App* disconnect = app.add_subcommand("disconnect", "Disconnect from the current Pear workspace");

    bool add_all = false;
    std::vector<std::filesystem::path> add_paths;
    CLI::App* add = app.add_subcommand("add", "Stage workspace changes");
    auto* add_paths_opt = add->add_option("paths", add_paths, "Paths to stage");
    auto* add_all_opt = add->add_flag("--all", add_all, "Stage all workspace changes");
    add_paths_opt->excludes(add_all_opt);
    add_all_opt->excludes(add_paths_opt);

    bool unstage_all = false;
    std::vector<std::filesystem::path> unstage_paths;
    CLI::App* unstage = app.add_subcommand("unstage", "Remove staged changes");
    auto* unstage_paths_opt = unstage->add_option("paths", unstage_paths, "Paths to unstage");
    auto* unstage_all_opt = unstage->add_flag("--all", unstage_all, "Remove all staged changes");
    unstage_paths_opt->excludes(unstage_all_opt);
    unstage_all_opt->excludes(unstage_paths_opt);

    CLI::App* update = app.add_subcommand("update", "Update Pear workspace metadata");

    bool json_ls = false;
    CLI::App* ls = app.add_subcommand("ls", "List files in the Pear workspace");
    ls->add_flag("--json", json_ls, "Output file list in JSON format");

    CLI::App* push = app.add_subcommand("push", "Push staged changes to the Pear workspace");

    std::vector<std::string> targets;
    CLI::App* pull = app.add_subcommand("pull", "Download files from the Pear workspace");
    pull->add_option("targets", targets, "Files to download")->required();

    bool json_status = false;
    CLI::App* status = app.add_subcommand("status", "Show workspace changes");
    status->add_flag("--json", json_status, "Output status in JSON format");

    size_t log_tail = 100;
    CLI::App* log = app.add_subcommand("log", "Show pear operation log");
    log->add_option("--tail", log_tail, "Show last N log lines");

    size_t cleanup_keep_versions = 1;
    bool cleanup_dry_run = false;
    CLI::App* cleanup = app.add_subcommand("cleanup", "Cleanup old file versions and unreferenced object files");
    cleanup->add_option("--keep-versions", cleanup_keep_versions, "Keep last N versions per file path");
    cleanup->add_flag("--dry-run", cleanup_dry_run, "Show cleanup plan without deleting anything");

    init->callback([&]() { pear::cli::run_init(workspace_path); });
    deinit->callback([&]() { pear::cli::run_deinit(); });
    connect->callback([&]() {
        if (!is_main && gu_address.empty()) {
            throw CLI::ValidationError("connect", "Specify --gu for non-main node");
        }
        pear::cli::run_connect(gu_address, listen_address, is_main);
    });
    disconnect->callback([&]() { pear::cli::run_disconnect(); });
    add->callback([&]() {
        if (!add_all && add_paths.empty()) {
            throw CLI::ValidationError("add", "Specify paths or use --all");
        }
        pear::cli::run_add(add_paths, add_all);
    });
    unstage->callback([&]() {
        if (!unstage_all && unstage_paths.empty()) {
            throw CLI::ValidationError("unstage", "Specify paths or use --all");
        }
        pear::cli::run_unstage(unstage_paths, unstage_all);
    });
    update->callback([&](){pear::cli::run_update(); });
    ls->callback([&](){pear::cli::run_ls(json_ls); });
    push->callback([&](){pear::cli::run_push(); });
    pull->callback([&](){pear::cli::run_pull(targets); });
    status->callback([&](){pear::cli::run_status(json_status); });
    log->callback([&](){pear::cli::run_log(log_tail); });
    cleanup->callback([&](){pear::cli::run_cleanup(cleanup_keep_versions, cleanup_dry_run); });

    try {
        app.require_subcommand(1);
        CLI11_PARSE(app, argc, argv);
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
