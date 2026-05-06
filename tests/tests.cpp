#include "tests_config.hpp"
#include "tests.hpp"

#include <iostream>

namespace pear::tests {

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
    EXPECT_EQ(1, 2);
}

#endif // PEAR_TEST_NET_PULL

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
