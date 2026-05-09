#include <pear/demon/demon.hpp>
#include <pear/fs/workspace.hpp>
#include <pear/db/sqlite_database.hpp>
#include <pear/net/node.hpp>

#include <memory>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <climits>
#ifdef __linux__
#include <linux/close_range.h>
#endif
#include <unistd.h>
#include <fstream>

namespace pear::demon {

namespace {

std::runtime_error make_errno_error(const std::string& message) {
    return std::runtime_error(message + ": " + std::strerror(errno));
}

void write_all(int fd, const char* data, std::size_t size) {
    std::size_t written = 0;

    while (written < size) {
        ssize_t result = ::write(fd, data + written, size - written);

        if (result == -1) {
            if (errno == EINTR) {
                continue;
            }
            throw make_errno_error("failed to write to status pipe");
        }

        written += static_cast<std::size_t>(result);
    }
}

void send_ok(int status_fd) {
    static constexpr char message[] = "ok\n";
    write_all(status_fd, message, sizeof(message) - 1);
}

void send_fail(int status_fd, const std::string& reason) {
    const std::string message = "fail: " + reason + "\n";
    write_all(status_fd, message.c_str(), message.size());
}

std::string read_status_message(int fd) {
    std::string message;
    char ch = '\0';
    while (true) {
        ssize_t result = ::read(fd, &ch, 1);
        if (result == 0) {
            break;
        }
        if (result == -1) {
            if (errno == EINTR) {
                continue;
            }
            throw make_errno_error("failed to read demon status");
        }
        if (ch == '\n') {
            break;
        }
        message.push_back(ch);
    }
    return message;
}

void wait_child(pid_t pid) {
    while (true) {
        int status = 0;
        pid_t result = ::waitpid(pid, &status, 0);

        if (result == -1) {
            if (errno == EINTR) {
                continue;
            }
            throw make_errno_error("waitpid failed");
        }

        return;
    }
}

void redirect_fd_or_close(int from_fd, int to_fd, const std::string& message){
    if (::dup2(from_fd, to_fd) == -1) {
        int saved_errno = errno;
        ::close(from_fd);
        errno = saved_errno;
        throw make_errno_error(message);
    }
}

void redirect_stdio_to_devnull() {
    // привязываем stdin stdout stderr к пустому специальному файлику в линуксе
    int null_fd = ::open("/dev/null", O_RDWR);
    if (null_fd == -1) {
        throw make_errno_error("failed to open /dev/null");
    }

    redirect_fd_or_close(null_fd, STDIN_FILENO, "failed to redirect stdin");
    redirect_fd_or_close(null_fd, STDOUT_FILENO, "failed to redirect stdout");
    redirect_fd_or_close(null_fd, STDERR_FILENO, "failed to redirect stderr");

    if (null_fd != STDIN_FILENO && null_fd != STDOUT_FILENO && null_fd != STDERR_FILENO) {
        ::close(null_fd);
    }
}

void close_all_fds_except(unsigned int preserved_fd) {
#ifdef __linux__
    // закрываем все дескрипторы кроме статусного
    // close_range - к сожалению есть зависимость от линукса((( 
    if (preserved_fd > 0) {
        if (::close_range(0, preserved_fd - 1, 0) == -1) {
            throw make_errno_error("close_range failed");
        }
    }
    if (preserved_fd < UINT_MAX) {
        if (::close_range(preserved_fd + 1, UINT_MAX, 0) == -1) {
            throw make_errno_error("close_range failed");
        }
    }
#else
    long max_fd = ::sysconf(_SC_OPEN_MAX);

    if (max_fd == -1) {
        max_fd = 1024;
    }

    for (int fd = 0; fd < max_fd; ++fd) {
        if (fd != static_cast<int>(preserved_fd)) {
            ::close(fd);
        }
    }
#endif
}

void write_pid_file(const std::filesystem::path& pid_file, pid_t pid) {
    int fd = ::open(pid_file.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (fd == -1) {
        throw make_errno_error("failed to create pid file");
    }

    try {
        const std::string pid_text = std::to_string(pid) + "\n";
        write_all(fd, pid_text.c_str(), pid_text.size());
        ::close(fd);
    } catch (...) {
        ::close(fd);
        ::unlink(pid_file.c_str());
        throw;
    }
}

volatile sig_atomic_t stop_requested = 0;

void handle_stop_signal(int /*signal_number*/) {
    stop_requested = 1;
}

void install_stop_handlers() {
    struct sigaction action {};
    action.sa_handler = handle_stop_signal;
    ::sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    if (::sigaction(SIGTERM, &action, nullptr) == -1) {
        throw make_errno_error("failed to install SIGTERM handler");
    }

    if (::sigaction(SIGINT, &action, nullptr) == -1) {
        throw make_errno_error("failed to install SIGINT handler");
    }
}

[[noreturn]] void daemon_child_main(const std::filesystem::path& workspace_root, const std::string& listen_address, bool is_main, int status_fd) {
    std::filesystem::path pid_file;
    bool pid_file_written = false;
    try {
        stop_requested = 0;

        if (::signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
            throw make_errno_error("failed to ignore SIGPIPE");
        }

        install_stop_handlers();

        if (::setsid() == -1) {
            throw make_errno_error("setsid failed");
        }

        pid_t second_pid = ::fork();
        if (second_pid == -1) {
            throw make_errno_error("second fork failed");
        }

        if (second_pid > 0) {
            ::_exit(EXIT_SUCCESS);
        }

        ::umask(0);

        if (::chdir("/") == -1) {
            throw make_errno_error("chdir failed");
        }

        // status_fd пока держим открытым, чтобы сообщить parent о результате старта.
        close_all_fds_except(status_fd);
        redirect_stdio_to_devnull();

        auto workspace = std::make_shared<pear::storage::Workspace>(pear::storage::Workspace::discover(workspace_root));
        auto database = std::make_shared<pear::db::SqliteDatabase>(workspace->get_meta_dir() / "peer.db");

        pid_file = workspace->get_meta_dir() / "demon.pid";
        write_pid_file(pid_file, ::getpid());
        pid_file_written = true;

        pear::net::Node node(database, workspace, is_main);
        node.start(listen_address, !is_main);

        send_ok(status_fd);
        ::close(status_fd);

        while (!stop_requested) {
            ::pause();
        }

        node.stop();

        if (pid_file_written) {
            std::filesystem::remove(pid_file);
        }

        ::_exit(EXIT_SUCCESS);
    } catch (const std::exception& exception) {
        try {
            send_fail(status_fd, exception.what());
        } catch (...) {
        }

        if (pid_file_written) {
            std::filesystem::remove(pid_file);
        }

        ::close(status_fd);
        ::_exit(EXIT_FAILURE);
    } catch (...) {
        try {
            send_fail(status_fd, "unknown error");
        } catch (...) {
        }

        if (pid_file_written) {
            std::filesystem::remove(pid_file);
        }

        ::close(status_fd);
        ::_exit(EXIT_FAILURE);
    }
}

pid_t read_pid_file(const std::filesystem::path& pid_file) {
    std::ifstream input(pid_file);
    if (!input.is_open()) {
        throw std::runtime_error("failed to open pid file");
    }

    long long raw_pid = 0;
    input >> raw_pid;

    if (!input || raw_pid <= 0) {
        throw std::runtime_error("pid file is corrupted");
    }

    return static_cast<pid_t>(raw_pid);
}

bool is_process_alive(pid_t pid) {
    if (pid <= 0) {
        return false;
    }

    return ::kill(pid, 0) == 0;
}

}  // namespace

void spawn(const std::filesystem::path& workspace_root, const std::string& listen_address, bool is_main) {
    if (is_alive(workspace_root)) {
        throw std::runtime_error("demon is already alive");
    }

    int pipe_fds[2];
    if (::pipe(pipe_fds) == -1) {
        throw make_errno_error("failed to create pipe");
    }

    pid_t first_pid = ::fork();
    if (first_pid == -1) {
        ::close(pipe_fds[0]);
        ::close(pipe_fds[1]);
        throw make_errno_error("first fork failed");
    }

    if (first_pid == 0) {
        ::close(pipe_fds[0]);
        daemon_child_main(workspace_root, listen_address, is_main, pipe_fds[1]);
    }

    ::close(pipe_fds[1]);

    std::string status_message;
    try {
        status_message = read_status_message(pipe_fds[0]);
        ::close(pipe_fds[0]);
        wait_child(first_pid);
    } catch (...) {
        ::close(pipe_fds[0]);
        wait_child(first_pid);
        throw;
    }

    if (status_message == "ok") {
        return;
    }

    if (status_message.rfind("fail: ", 0) == 0) {
        throw std::runtime_error(status_message.substr(6));
    }

    if (status_message.empty()) {
        throw std::runtime_error("demon terminated before reporting startup status");
    }

    throw std::runtime_error("unexpected demon status: " + status_message);
}


bool is_alive(const std::filesystem::path& workspace_root) {
    std::filesystem::path pid_file = pear::storage::Workspace::discover(workspace_root).get_meta_dir() / "demon.pid";

    if (!std::filesystem::exists(pid_file)) {
        return false;
    }

    pid_t pid = 0;

    try {
        pid = read_pid_file(pid_file);
    } catch (...) {
        std::filesystem::remove(pid_file);
        return false;
    }

    if (is_process_alive(pid)) {
        return true;
    }

    std::filesystem::remove(pid_file);
    return false;
}

void kill(const std::filesystem::path& workspace_root) {
    std::filesystem::path pid_file =
        pear::storage::Workspace::discover(workspace_root).get_meta_dir() / "demon.pid";

    if (!is_alive(workspace_root)) {
        throw std::runtime_error("demon is not alive");
    }

    pid_t pid = read_pid_file(pid_file);

    if (::kill(pid, SIGTERM) == -1) {
        throw make_errno_error("failed to send SIGTERM to demon");
    }
}

}  // namespace pear::demon