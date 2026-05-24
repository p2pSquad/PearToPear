#include "tests_config.hpp"
#include "tests.hpp"

#include <iostream>
#include <algorithm>

namespace pear::tests {

namespace {

std::vector<std::string> sorted(std::vector<std::string> values) {
    std::sort(values.begin(), values.end());
    return values;
}

std::string object_hash_for(const Ls& ls, const std::string& path) {
    for (const auto& file : ls.files) {
        if (file.path == path) {
            return file.object_hash;
        }
    }

    return {};
}

} // namespace

#ifdef PEAR_TEST_ADD_FILE

TEST(add, file) {
    Repo repo;

    EXPECT_EQ(repo.init().code, 0);

    repo.write_file("a.txt", "hello\n");

    EXPECT_EQ(repo.add({"a.txt"}).code, 0);

    const Status status = repo.status();

    EXPECT_EQ(staged_paths(status), (std::vector<std::string>{"a.txt"}));
    EXPECT_EQ(status.untracked, std::vector<std::string>{});
}

#endif // PEAR_TEST_ADD_FILE

#ifdef PEAR_TEST_ADD_SAME_NAMES

TEST(add, same_names) {
    Repo repo;

    EXPECT_EQ(repo.init().code, 0);

    repo.write_file("a/file.txt", "one\n");
    repo.write_file("b/file.txt", "two\n");

    EXPECT_EQ(repo.add({"a/file.txt", "b/file.txt"}).code, 0);

    EXPECT_EQ(staged_paths(repo.status()), (std::vector<std::string>{"a/file.txt", "b/file.txt"}));
}

#endif // PEAR_TEST_ADD_SAME_NAMES

#ifdef PEAR_TEST_ADD_DIRECTORY

TEST(add, directory) {
    Repo repo;

    EXPECT_EQ(repo.init().code, 0);

    repo.write_file("dir/a.txt", "a\n");
    repo.write_file("dir/sub/b.txt", "b\n");

    EXPECT_EQ(repo.add({"dir"}).code, 0);

    EXPECT_EQ(staged_paths(repo.status()), (std::vector<std::string>{"dir/a.txt", "dir/sub/b.txt"}));
}

#endif // PEAR_TEST_ADD_DIRECTORY

#ifdef PEAR_TEST_LOG

TEST(log, basic_tail) {
    Repo repo;

    EXPECT_EQ(repo.init().code, 0);

    repo.write_file("a.txt", "hello\n");
    EXPECT_EQ(repo.add({"a.txt"}).code, 0);
    EXPECT_EQ(repo.status().staged.size(), 1u);

    const CommandResult full_log = repo.log();
    EXPECT_EQ(full_log.code, 0) << full_log.out << full_log.err;
    EXPECT_FALSE(full_log.out.empty()) << full_log.err;

    const CommandResult tail_log = repo.log_tail(1);
    EXPECT_EQ(tail_log.code, 0) << tail_log.out << tail_log.err;

    size_t non_empty_lines = 0;
    std::stringstream stream(tail_log.out);
    std::string line;

    while (std::getline(stream, line)) {
        if (!line.empty()) {
            ++non_empty_lines;
        }
    }

    EXPECT_LE(non_empty_lines, 1u);
}

#endif // PEAR_TEST_LOG

#ifdef PEAR_TEST_NET_UPDATE

TEST(net, update_metadata) {
    Repo main;
    Repo peer;

    const std::string main_address = local_address();
    const std::string peer_address = local_address();

    EXPECT_EQ(main.init().code, 0);
    EXPECT_EQ(peer.init().code, 0);

    EXPECT_EQ(main.connect_main(main_address).code, 0);
    wait_network();

    EXPECT_EQ(peer.connect(main_address, peer_address).code, 0);
    wait_network();

    main.write_file("a.txt", "hello from main\n");

    EXPECT_EQ(main.add({"a.txt"}).code, 0);
    EXPECT_EQ(main.push().code, 0);

    EXPECT_EQ(peer.update().code, 0);

    EXPECT_EQ(file_paths(peer.ls()), (std::vector<std::string>{"a.txt"}));
    EXPECT_TRUE(peer.exists("a.txt.empty"));
    EXPECT_EQ(peer.status().missing, std::vector<std::string>{});

    EXPECT_EQ(peer.disconnect().code, 0);
    EXPECT_EQ(main.disconnect().code, 0);
}

#endif // PEAR_TEST_NET_UPDATE

#ifdef PEAR_TEST_NET_PULL

TEST(net, pull_file) {
    Repo main;
    Repo peer;

    const std::string main_address = local_address();
    const std::string peer_address = local_address();

    EXPECT_EQ(main.init().code, 0);
    EXPECT_EQ(peer.init().code, 0);

    EXPECT_EQ(main.connect_main(main_address).code, 0);
    wait_network();

    EXPECT_EQ(peer.connect(main_address, peer_address).code, 0);
    wait_network();

    main.write_file("dir/a.txt", "hello from main\n");

    EXPECT_EQ(main.add({"dir/a.txt"}).code, 0);
    EXPECT_EQ(main.push().code, 0);

    EXPECT_EQ(peer.update().code, 0);

    EXPECT_EQ(file_paths(peer.ls()), (std::vector<std::string>{"dir/a.txt"}));
    EXPECT_TRUE(peer.exists("dir/a.txt.empty"));
    EXPECT_EQ(peer.status().missing, std::vector<std::string>{});

    EXPECT_EQ(peer.pull({"dir/a.txt"}).code, 0);

    EXPECT_TRUE(peer.exists("dir/a.txt"));
    EXPECT_FALSE(peer.exists("dir/a.txt.empty"));
    EXPECT_EQ(peer.read_file("dir/a.txt"), "hello from main\n");
    EXPECT_EQ(peer.status().missing, std::vector<std::string>{});

    EXPECT_EQ(peer.disconnect().code, 0);
    EXPECT_EQ(main.disconnect().code, 0);
}

#endif // PEAR_TEST_NET_PULL

#ifdef PEAR_TEST_ADD_ALL_SKIPS_PEER_DIRECTORY

TEST(add, all_skips_peer_directory) {
    Repo repo;

    EXPECT_EQ(repo.init().code, 0);

    repo.write_file("a.txt", "a\n");
    repo.write_file("dir/b.txt", "b\n");

    EXPECT_EQ(repo.add_all().code, 0);

    const Status status = repo.status();

    EXPECT_EQ(sorted(staged_paths(status)), (std::vector<std::string>{
        "a.txt",
        "dir/b.txt"
    }));

    for (const auto& path : staged_paths(status)) {
        EXPECT_EQ(path.find(".peer"), std::string::npos);
    }
}

#endif // PEAR_TEST_ADD_ALL_SKIPS_PEER_DIRECTORY

#ifdef PEAR_TEST_ADD_MODIFIED_AFTER_STAGING

TEST(add, modified_after_staging) {
    Repo repo;

    EXPECT_EQ(repo.init().code, 0);

    repo.write_file("a.txt", "old\n");

    EXPECT_EQ(repo.add({"a.txt"}).code, 0);

    repo.write_file("a.txt", "new\n");

    const Status status = repo.status();

    EXPECT_EQ(staged_paths(status), (std::vector<std::string>{"a.txt"}));
    EXPECT_EQ(status.modified_after_staging, (std::vector<std::string>{"a.txt"}));
}

#endif // PEAR_TEST_ADD_MODIFIED_AFTER_STAGING

#ifdef PEAR_TEST_UNSTAGE_DIRECTORY

TEST(unstage, directory) {
    Repo repo;

    EXPECT_EQ(repo.init().code, 0);

    repo.write_file("dir/a.txt", "a\n");
    repo.write_file("dir/sub/b.txt", "b\n");

    EXPECT_EQ(repo.add({"dir"}).code, 0);
    EXPECT_EQ(repo.unstage({"dir"}).code, 0);

    const Status status = repo.status();

    EXPECT_TRUE(staged_paths(status).empty());
    EXPECT_EQ(status.untracked, (std::vector<std::string>{"dir/"}));
}

#endif // PEAR_TEST_UNSTAGE_DIRECTORY

#ifdef PEAR_TEST_NET_PULL_DIRECTORY

TEST(net, pull_directory_restores_tree) {
    Repo main;
    Repo peer;

    const std::string main_address = local_address();
    const std::string peer_address = local_address();

    EXPECT_EQ(main.init().code, 0);
    EXPECT_EQ(peer.init().code, 0);

    EXPECT_EQ(main.connect_main(main_address).code, 0);
    wait_network();

    EXPECT_EQ(peer.connect(main_address, peer_address).code, 0);
    wait_network();

    main.write_file("dir/a.txt", "a from main\n");
    main.write_file("dir/sub/b.txt", "b from main\n");

    EXPECT_EQ(main.add({"dir"}).code, 0);
    EXPECT_EQ(main.push().code, 0);

    EXPECT_EQ(peer.update().code, 0);

    EXPECT_EQ(sorted(file_paths(peer.ls())), (std::vector<std::string>{
        "dir/a.txt",
        "dir/sub/b.txt"
    }));

    EXPECT_TRUE(peer.exists("dir/a.txt.empty"));
    EXPECT_TRUE(peer.exists("dir/sub/b.txt.empty"));

    EXPECT_EQ(peer.pull({"dir"}).code, 0);

    EXPECT_TRUE(peer.exists("dir/a.txt"));
    EXPECT_TRUE(peer.exists("dir/sub/b.txt"));

    EXPECT_FALSE(peer.exists("dir/a.txt.empty"));
    EXPECT_FALSE(peer.exists("dir/sub/b.txt.empty"));

    EXPECT_EQ(peer.read_file("dir/a.txt"), "a from main\n");
    EXPECT_EQ(peer.read_file("dir/sub/b.txt"), "b from main\n");

    EXPECT_EQ(peer.disconnect().code, 0);
    EXPECT_EQ(main.disconnect().code, 0);
}

#endif // PEAR_TEST_NET_PULL_DIRECTORY

#ifdef PEAR_TEST_NET_PULL_MODIFY_PUSH

TEST(net, pull_modify_push) {
    Repo main;
    Repo peer;
    Repo third;

    const std::string main_address = local_address();
    const std::string peer_address = local_address();
    const std::string third_address = local_address();

    EXPECT_EQ(main.init().code, 0);
    EXPECT_EQ(peer.init().code, 0);
    EXPECT_EQ(third.init().code, 0);

    EXPECT_EQ(main.connect_main(main_address).code, 0);
    wait_network();

    EXPECT_EQ(peer.connect(main_address, peer_address).code, 0);
    wait_network();

    EXPECT_EQ(third.connect(main_address, third_address).code, 0);
    wait_network();

    main.write_file("file.txt", "original\n");

    EXPECT_EQ(main.add({"file.txt"}).code, 0);
    EXPECT_EQ(main.push().code, 0);

    EXPECT_EQ(peer.update().code, 0);
    EXPECT_EQ(peer.pull({"file.txt"}).code, 0);

    EXPECT_EQ(peer.read_file("file.txt"), "original\n");

    peer.write_file("file.txt", "changed by peer\n");

    EXPECT_EQ(peer.add({"file.txt"}).code, 0);
    EXPECT_EQ(peer.push().code, 0);

    EXPECT_EQ(third.update().code, 0);
    EXPECT_EQ(third.pull({"file.txt"}).code, 0);

    EXPECT_EQ(third.read_file("file.txt"), "changed by peer\n");

    EXPECT_EQ(third.disconnect().code, 0);
    EXPECT_EQ(peer.disconnect().code, 0);
    EXPECT_EQ(main.disconnect().code, 0);
}

#endif // PEAR_TEST_NET_PULL_MODIFY_PUSH

#ifdef PEAR_TEST_NET_THREE_DEVICES_PUSH_DIFFERENT_FILES

TEST(net, three_devices_push_different_files) {
    Repo main;
    Repo peer1;
    Repo peer2;

    const std::string main_address = local_address();
    const std::string peer1_address = local_address();
    const std::string peer2_address = local_address();

    EXPECT_EQ(main.init().code, 0);
    EXPECT_EQ(peer1.init().code, 0);
    EXPECT_EQ(peer2.init().code, 0);

    EXPECT_EQ(main.connect_main(main_address).code, 0);
    wait_network();

    EXPECT_EQ(peer1.connect(main_address, peer1_address).code, 0);
    wait_network();

    EXPECT_EQ(peer2.connect(main_address, peer2_address).code, 0);
    wait_network();

    main.write_file("main.txt", "from main\n");

    EXPECT_EQ(main.add({"main.txt"}).code, 0);
    EXPECT_EQ(main.push().code, 0);

    EXPECT_EQ(peer1.update().code, 0);

    peer1.write_file("peer1.txt", "from peer1\n");

    EXPECT_EQ(peer1.add({"peer1.txt"}).code, 0);
    EXPECT_EQ(peer1.push().code, 0);

    EXPECT_EQ(peer2.update().code, 0);

    peer2.write_file("peer2.txt", "from peer2\n");

    EXPECT_EQ(peer2.add({"peer2.txt"}).code, 0);
    EXPECT_EQ(peer2.push().code, 0);

    EXPECT_EQ(main.update().code, 0);
    EXPECT_EQ(peer1.update().code, 0);
    EXPECT_EQ(peer2.update().code, 0);

    const std::vector<std::string> expected_paths = {
        "main.txt",
        "peer1.txt",
        "peer2.txt"
    };

    EXPECT_EQ(sorted(file_paths(main.ls())), expected_paths);
    EXPECT_EQ(sorted(file_paths(peer1.ls())), expected_paths);
    EXPECT_EQ(sorted(file_paths(peer2.ls())), expected_paths);

    EXPECT_EQ(peer2.disconnect().code, 0);
    EXPECT_EQ(peer1.disconnect().code, 0);
    EXPECT_EQ(main.disconnect().code, 0);
}

#endif // PEAR_TEST_NET_THREE_DEVICES_PUSH_DIFFERENT_FILES

#ifdef PEAR_TEST_NET_SAME_CONTENT_DEDUPLICATES_OBJECTS

TEST(net, same_content_deduplicates_objects) {
    Repo main;
    Repo peer;

    const std::string main_address = local_address();
    const std::string peer_address = local_address();

    EXPECT_EQ(main.init().code, 0);
    EXPECT_EQ(peer.init().code, 0);

    EXPECT_EQ(main.connect_main(main_address).code, 0);
    wait_network();

    EXPECT_EQ(peer.connect(main_address, peer_address).code, 0);
    wait_network();

    main.write_file("a/copy1.txt", "same content\n");
    main.write_file("b/copy2.txt", "same content\n");
    main.write_file("c/copy3.txt", "same content\n");

    EXPECT_EQ(main.add({"a", "b", "c"}).code, 0);

    const Status main_status = main.status();

    ASSERT_EQ(main_status.staged.size(), 3);

    EXPECT_EQ(main_status.staged[0].object_hash, main_status.staged[1].object_hash);
    EXPECT_EQ(main_status.staged[1].object_hash, main_status.staged[2].object_hash);

    EXPECT_EQ(main.object_count(), 1);

    peer.write_file("a/copy1.txt", "same content\n");
    peer.write_file("b/copy2.txt", "same content\n");
    peer.write_file("c/copy3.txt", "same content\n");

    EXPECT_EQ(peer.add({"a", "b", "c"}).code, 0);
    EXPECT_EQ(peer.object_count(), 1);

    EXPECT_EQ(main.push().code, 0);

    EXPECT_EQ(peer.update().code, 0);

    const Ls peer_ls_before_pull = peer.ls();

    EXPECT_EQ(sorted(file_paths(peer_ls_before_pull)), (std::vector<std::string>{
        "a/copy1.txt",
        "b/copy2.txt",
        "c/copy3.txt"
    }));

    const std::string first_hash = object_hash_for(peer_ls_before_pull, "a/copy1.txt");

    ASSERT_FALSE(first_hash.empty());
    EXPECT_EQ(object_hash_for(peer_ls_before_pull, "b/copy2.txt"), first_hash);
    EXPECT_EQ(object_hash_for(peer_ls_before_pull, "c/copy3.txt"), first_hash);

    EXPECT_EQ(peer.pull({"a/copy1.txt", "b/copy2.txt", "c/copy3.txt"}).code, 0);

    EXPECT_EQ(peer.read_file("a/copy1.txt"), "same content\n");
    EXPECT_EQ(peer.read_file("b/copy2.txt"), "same content\n");
    EXPECT_EQ(peer.read_file("c/copy3.txt"), "same content\n");

    EXPECT_EQ(peer.object_count(), 1);

    EXPECT_EQ(peer.disconnect().code, 0);
    EXPECT_EQ(main.disconnect().code, 0);
}

#endif // PEAR_TEST_NET_SAME_CONTENT_DEDUPLICATES_OBJECTS

#ifdef PEAR_TEST_NET_OFFLINE_PEER_PULL_FAILS_CLEANLY

TEST(net, offline_peer_pull_fails_cleanly) {
    Repo main;
    Repo peer;

    const std::string main_address = local_address();
    const std::string peer_address = local_address();

    EXPECT_EQ(main.init().code, 0);
    EXPECT_EQ(peer.init().code, 0);

    EXPECT_EQ(main.connect_main(main_address).code, 0);
    wait_network();

    EXPECT_EQ(peer.connect(main_address, peer_address).code, 0);
    wait_network();

    main.write_file("file.txt", "only on main\n");

    EXPECT_EQ(main.add({"file.txt"}).code, 0);
    EXPECT_EQ(main.push().code, 0);

    EXPECT_EQ(peer.update().code, 0);

    EXPECT_TRUE(peer.exists("file.txt.empty"));
    EXPECT_FALSE(peer.exists("file.txt"));

    EXPECT_EQ(main.disconnect().code, 0);
    wait_network();

    const CommandResult pull_result = peer.pull({"file.txt"});

    EXPECT_NE(pull_result.code, 0);
    EXPECT_FALSE(peer.exists("file.txt"));
    EXPECT_TRUE(peer.exists("file.txt.empty"));

    EXPECT_EQ(peer.disconnect().code, 0);
}

#endif // PEAR_TEST_NET_OFFLINE_PEER_PULL_FAILS_CLEANLY

#ifdef PEAR_TEST_NET_DIRECTORY_MODIFY_PUSH_PULL

TEST(net, directory_modify_push_pull) {
    Repo main;
    Repo peer;
    Repo third;

    const std::string main_address = local_address();
    const std::string peer_address = local_address();
    const std::string third_address = local_address();

    EXPECT_EQ(main.init().code, 0);
    EXPECT_EQ(peer.init().code, 0);
    EXPECT_EQ(third.init().code, 0);

    EXPECT_EQ(main.connect_main(main_address).code, 0);
    wait_network();

    EXPECT_EQ(peer.connect(main_address, peer_address).code, 0);
    wait_network();

    EXPECT_EQ(third.connect(main_address, third_address).code, 0);
    wait_network();

    main.write_file("project/readme.txt", "version 1\n");
    main.write_file("project/src/main.cpp", "int main() { return 0; }\n");

    EXPECT_EQ(main.add({"project"}).code, 0);
    EXPECT_EQ(main.push().code, 0);

    EXPECT_EQ(peer.update().code, 0);
    EXPECT_EQ(peer.pull({"project"}).code, 0);

    EXPECT_EQ(peer.read_file("project/readme.txt"), "version 1\n");
    EXPECT_EQ(peer.read_file("project/src/main.cpp"), "int main() { return 0; }\n");

    peer.write_file("project/readme.txt", "version 2 from peer\n");
    peer.write_file("project/src/helper.cpp", "void helper() {}\n");

    EXPECT_EQ(peer.add({"project"}).code, 0);
    EXPECT_EQ(peer.push().code, 0);

    EXPECT_EQ(third.update().code, 0);

    EXPECT_EQ(sorted(file_paths(third.ls())), (std::vector<std::string>{
        "project/readme.txt",
        "project/src/helper.cpp",
        "project/src/main.cpp"
    }));

    EXPECT_TRUE(third.exists("project/readme.txt.empty"));
    EXPECT_TRUE(third.exists("project/src/helper.cpp.empty"));
    EXPECT_TRUE(third.exists("project/src/main.cpp.empty"));

    EXPECT_EQ(third.pull({"project"}).code, 0);

    EXPECT_EQ(third.read_file("project/readme.txt"), "version 2 from peer\n");
    EXPECT_EQ(third.read_file("project/src/helper.cpp"), "void helper() {}\n");
    EXPECT_EQ(third.read_file("project/src/main.cpp"), "int main() { return 0; }\n");

    EXPECT_FALSE(third.exists("project/readme.txt.empty"));
    EXPECT_FALSE(third.exists("project/src/helper.cpp.empty"));
    EXPECT_FALSE(third.exists("project/src/main.cpp.empty"));

    EXPECT_EQ(third.disconnect().code, 0);
    EXPECT_EQ(peer.disconnect().code, 0);
    EXPECT_EQ(main.disconnect().code, 0);
}

#endif // PEAR_TEST_NET_DIRECTORY_MODIFY_PUSH_PULL

#ifdef PEAR_TEST_NET_REPEATED_UPDATE_PULL_IS_IDEMPOTENT

TEST(net, repeated_update_pull_is_idempotent) {
    Repo main;
    Repo peer;

    const std::string main_address = local_address();
    const std::string peer_address = local_address();

    EXPECT_EQ(main.init().code, 0);
    EXPECT_EQ(peer.init().code, 0);

    EXPECT_EQ(main.connect_main(main_address).code, 0);
    wait_network();

    EXPECT_EQ(peer.connect(main_address, peer_address).code, 0);
    wait_network();

    main.write_file("lib/a.txt", "a v1\n");
    main.write_file("lib/b.txt", "b v1\n");
    main.write_file("lib/nested/c.txt", "c v1\n");

    EXPECT_EQ(main.add({"lib"}).code, 0);
    EXPECT_EQ(main.push().code, 0);

    EXPECT_EQ(peer.update().code, 0);
    EXPECT_EQ(peer.pull({"lib"}).code, 0);

    EXPECT_EQ(sorted(file_paths(peer.ls())), (std::vector<std::string>{
        "lib/a.txt",
        "lib/b.txt",
        "lib/nested/c.txt"
    }));

    EXPECT_EQ(peer.read_file("lib/a.txt"), "a v1\n");
    EXPECT_EQ(peer.read_file("lib/b.txt"), "b v1\n");
    EXPECT_EQ(peer.read_file("lib/nested/c.txt"), "c v1\n");

    EXPECT_FALSE(peer.exists("lib/a.txt.empty"));
    EXPECT_FALSE(peer.exists("lib/b.txt.empty"));
    EXPECT_FALSE(peer.exists("lib/nested/c.txt.empty"));

    EXPECT_EQ(peer.update().code, 0);
    EXPECT_EQ(peer.pull({"lib"}).code, 0);

    EXPECT_EQ(sorted(file_paths(peer.ls())), (std::vector<std::string>{
        "lib/a.txt",
        "lib/b.txt",
        "lib/nested/c.txt"
    }));

    EXPECT_EQ(peer.read_file("lib/a.txt"), "a v1\n");
    EXPECT_EQ(peer.read_file("lib/b.txt"), "b v1\n");
    EXPECT_EQ(peer.read_file("lib/nested/c.txt"), "c v1\n");

    main.write_file("lib/a.txt", "a v2\n");
    main.write_file("lib/new.txt", "new file\n");

    EXPECT_EQ(main.add({"lib"}).code, 0);
    EXPECT_EQ(main.push().code, 0);

    EXPECT_EQ(peer.update().code, 0);

    EXPECT_EQ(sorted(file_paths(peer.ls())), (std::vector<std::string>{
        "lib/a.txt",
        "lib/b.txt",
        "lib/nested/c.txt",
        "lib/new.txt"
    }));

    EXPECT_EQ(peer.pull({"lib"}).code, 0);

    EXPECT_EQ(peer.read_file("lib/a.txt"), "a v2\n");
    EXPECT_EQ(peer.read_file("lib/b.txt"), "b v1\n");
    EXPECT_EQ(peer.read_file("lib/nested/c.txt"), "c v1\n");
    EXPECT_EQ(peer.read_file("lib/new.txt"), "new file\n");

    EXPECT_FALSE(peer.exists("lib/a.txt.empty"));
    EXPECT_FALSE(peer.exists("lib/b.txt.empty"));
    EXPECT_FALSE(peer.exists("lib/nested/c.txt.empty"));
    EXPECT_FALSE(peer.exists("lib/new.txt.empty"));

    EXPECT_EQ(peer.disconnect().code, 0);
    EXPECT_EQ(main.disconnect().code, 0);
}

#endif // PEAR_TEST_NET_REPEATED_UPDATE_PULL_IS_IDEMPOTENT

} // namespace pear::tests

int main(int argc, char** argv) {
    bool list_tests = false;

    for (int arg_index = 1; arg_index < argc; ++arg_index) {
        if (std::string(argv[arg_index]) == "--gtest_list_tests") {
            list_tests = true;
            break;
        }
    }

    ::testing::InitGoogleTest(&argc, argv);

    if (!list_tests && argc < 2) {
        std::cerr << "pear binary path is required\n";
        return 1;
    }

    if (argc >= 2) {
        pear::tests::pear_binary = argv[1];
    }

    return RUN_ALL_TESTS();
}
