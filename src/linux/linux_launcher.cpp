#include "linux_launcher.h"
#include "desktop_entry.h"

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <random>
#include <sstream>

#ifndef _WIN32
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace broapps::linux_backend {

namespace {

std::string sanitize_unit_name(std::string_view name) {
    std::string out;
    for (char c : name) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_') {
            out.push_back(c);
        } else {
            out.push_back('_');
        }
    }
    if (out.empty()) out = "app";
    return out;
}

std::string generate_random_suffix() {
    static std::mt19937_64 rng(std::random_device{}());
    uint64_t val = rng();
    std::ostringstream ss;
    ss << std::hex << val;
    return ss.str().substr(0, 8);
}

std::vector<std::string> find_terminal_command() {
    static const char* candidates[] = {
        "x-terminal-emulator",
        "foot",
        "kitty",
        "alacritty",
        "gnome-terminal",
        "konsole",
        "xfce4-terminal",
        "xterm"
    };

    const char* path_env = std::getenv("PATH");
    if (!path_env) path_env = "/usr/bin:/bin";

    std::istringstream stream(path_env);
    std::string dir;
    std::vector<std::string> path_dirs;
    while (std::getline(stream, dir, ':')) {
        if (!dir.empty()) path_dirs.push_back(dir);
    }

    for (const char* term : candidates) {
        for (const auto& d : path_dirs) {
            std::filesystem::path p = std::filesystem::path(d) / term;
            std::error_code ec;
            if (std::filesystem::exists(p, ec)) {
                return {p.string(), "-e"};
            }
        }
    }
    return {};
}

}  // namespace

LinuxProcessHandle::LinuxProcessHandle(
    uint64_t launch_id,
    int64_t pid,
    std::string scope_id,
    bool is_systemd_scope)
    : ProcessHandleBase(launch_id, pid, std::move(scope_id)),
      is_systemd_scope_(is_systemd_scope) {}

LinuxProcessHandle::~LinuxProcessHandle() = default;

bool LinuxProcessHandle::terminate() {
#ifndef _WIN32
    int64_t p = pid_.load();
    if (p <= 0 || !is_running_.load()) return false;

    if (is_systemd_scope_ && !scope_id_.empty()) {
        std::string cmd = "systemctl --user stop " + scope_id_ + " >/dev/null 2>&1";
        std::system(cmd.c_str());
    }

    // Send SIGTERM to process group if negative, else direct pid
    ::kill(-static_cast<pid_t>(p), SIGTERM);
    ::kill(static_cast<pid_t>(p), SIGTERM);
    return true;
#else
    return false;
#endif
}

bool LinuxProcessHandle::kill() {
#ifndef _WIN32
    int64_t p = pid_.load();
    if (p <= 0 || !is_running_.load()) return false;

    if (is_systemd_scope_ && !scope_id_.empty()) {
        std::string cmd = "systemctl --user kill --signal=SIGKILL " + scope_id_ + " >/dev/null 2>&1";
        std::system(cmd.c_str());
    }

    ::kill(-static_cast<pid_t>(p), SIGKILL);
    ::kill(static_cast<pid_t>(p), SIGKILL);
    return true;
#else
    return false;
#endif
}

LinuxLauncher::LinuxLauncher(LauncherConfig config)
    : config_(config) {}

LinuxLauncher::~LinuxLauncher() = default;

bool LinuxLauncher::is_systemd_user_available() {
#ifndef _WIN32
    static int cached_status = -1;
    if (cached_status != -1) return cached_status == 1;

    // Fast check: is systemd-run available and can we talk to systemd user manager?
    int ret = std::system("systemd-run --user --scope true >/dev/null 2>&1");
    cached_status = (ret == 0) ? 1 : 0;
    return cached_status == 1;
#else
    return false;
#endif
}

std::shared_ptr<ProcessHandle> LinuxLauncher::launch(
    const AppInfo& app,
    const LaunchScope& scope) {

    auto argv = expand_exec(app, scope);
    if (argv.empty()) {
        uint64_t lid = next_launch_id_++;
        events_.push(AppFailed{lid, app.id, "Empty command line or invalid desktop entry", std::chrono::system_clock::now()});
        auto h = std::make_shared<LinuxProcessHandle>(lid, 0, "", false);
        h->notify_exited(127);
        return h;
    }

    bool terminal = app.is_terminal || scope.terminal;
    return launch_argv(app.id, argv, scope, terminal);
}

std::shared_ptr<ProcessHandle> LinuxLauncher::launch_executable(
    const std::string& exec_path,
    const LaunchScope& scope) {

    std::vector<std::string> argv;
    argv.push_back(exec_path);
    for (const auto& a : scope.arguments) {
        argv.push_back(a);
    }
    for (const auto& f : scope.files_to_open) {
        argv.push_back(f);
    }

    std::string app_id;
    try {
        app_id = std::filesystem::path(exec_path).filename().string();
    } catch (...) {
        app_id = exec_path;
    }

    return launch_argv(app_id, argv, scope, scope.terminal);
}

std::shared_ptr<ProcessHandle> LinuxLauncher::launch_argv(
    const std::string& app_id,
    const std::vector<std::string>& argv,
    const LaunchScope& scope,
    bool is_terminal) {

    uint64_t lid = next_launch_id_++;

#ifndef _WIN32
    std::vector<std::string> final_argv;

    if (is_terminal) {
        auto term_cmd = find_terminal_command();
        if (!term_cmd.empty()) {
            final_argv.insert(final_argv.end(), term_cmd.begin(), term_cmd.end());
        }
    }
    final_argv.insert(final_argv.end(), argv.begin(), argv.end());

    std::string scope_id;
    bool use_systemd = config_.prefer_systemd_scope && is_systemd_user_available();

    if (use_systemd) {
        std::string unit_base = scope.scope_name_hint.empty()
            ? ("app-" + sanitize_unit_name(app_id))
            : sanitize_unit_name(scope.scope_name_hint);
        scope_id = unit_base + "-" + generate_random_suffix() + ".scope";

        std::vector<std::string> sysd_argv = {
            "systemd-run",
            "--user",
            "--scope",
            "--unit=" + scope_id,
            "--quiet",
            "--"
        };
        sysd_argv.insert(sysd_argv.end(), final_argv.begin(), final_argv.end());
        final_argv = std::move(sysd_argv);
    }

    // Set up pipe to detect exec failures
    int pipefd[2];
    if (pipe2(pipefd, O_CLOEXEC) < 0) {
        events_.push(AppFailed{lid, app_id, std::strerror(errno), std::chrono::system_clock::now()});
        auto h = std::make_shared<LinuxProcessHandle>(lid, 0, scope_id, use_systemd);
        h->notify_exited(127);
        return h;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        events_.push(AppFailed{lid, app_id, std::strerror(errno), std::chrono::system_clock::now()});
        auto h = std::make_shared<LinuxProcessHandle>(lid, 0, scope_id, use_systemd);
        h->notify_exited(127);
        return h;
    }

    if (pid == 0) {
        // Child process
        close(pipefd[0]);

        // Create new process group
        setpgid(0, 0);

        if (scope.detached) {
            setsid();
            int devnull = open("/dev/null", O_RDWR);
            if (devnull >= 0) {
                dup2(devnull, STDIN_FILENO);
                dup2(devnull, STDOUT_FILENO);
                dup2(devnull, STDERR_FILENO);
                if (devnull > STDERR_FILENO) close(devnull);
            }
        }

        // Apply working directory
        if (!scope.working_directory.empty()) {
            if (chdir(scope.working_directory.c_str()) != 0) {
                // Warning/ignored
            }
        }

        // Apply environment variables
        for (const auto& [k, v] : scope.environment) {
            setenv(k.c_str(), v.c_str(), 1);
        }

        // Prepare execv arguments
        std::vector<char*> c_argv;
        c_argv.reserve(final_argv.size() + 1);
        for (auto& s : final_argv) {
            c_argv.push_back(s.data());
        }
        c_argv.push_back(nullptr);

        execvp(c_argv[0], c_argv.data());

        // If execvp failed:
        int err = errno;
        ssize_t written = write(pipefd[1], &err, sizeof(err));
        (void)written;
        close(pipefd[1]);
        _exit(127);
    }

    // Parent process
    close(pipefd[1]);

    int child_err = 0;
    ssize_t bytes_read = read(pipefd[0], &child_err, sizeof(child_err));
    close(pipefd[0]);

    if (bytes_read > 0) {
        // execvp failed
        waitpid(pid, nullptr, 0);
        std::string err_msg = "exec failed: ";
        err_msg += std::strerror(child_err);
        events_.push(AppFailed{lid, app_id, err_msg, std::chrono::system_clock::now()});
        auto h = std::make_shared<LinuxProcessHandle>(lid, 0, scope_id, use_systemd);
        h->notify_exited(127);
        return h;
    }

    auto handle = std::make_shared<LinuxProcessHandle>(lid, pid, scope_id, use_systemd);

    events_.push(AppStarted{
        lid,
        app_id,
        static_cast<int64_t>(pid),
        scope_id,
        std::chrono::system_clock::now()
    });

    // Start background monitoring thread
    std::thread([this, handle, lid, app_id, pid]() {
        int status = 0;
        pid_t res = waitpid(pid, &status, 0);
        if (res > 0) {
            int exit_code = 0;
            bool killed = false;
            std::string sig_name;

            if (WIFEXITED(status)) {
                exit_code = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                int sig = WTERMSIG(status);
                exit_code = 128 + sig;
                killed = true;
                const char* desc = strsignal(sig);
                sig_name = desc ? desc : ("Signal " + std::to_string(sig));
            }

            events_.push(AppExited{
                lid,
                app_id,
                static_cast<int64_t>(pid),
                exit_code,
                killed,
                sig_name,
                std::chrono::system_clock::now()
            });

            handle->notify_exited(exit_code);
        }
    }).detach();

    return handle;
#else
    (void)argv;
    (void)scope;
    (void)is_terminal;
    events_.push(AppFailed{lid, app_id, "LinuxLauncher not supported on Windows", std::chrono::system_clock::now()});
    auto h = std::make_shared<LinuxProcessHandle>(lid, 0, "", false);
    h->notify_exited(1);
    return h;
#endif
}

}  // namespace broapps::linux_backend

#if defined(__linux__)
namespace broapps {

std::unique_ptr<AppLauncher> AppLauncher::create(const LauncherConfig& config) {
    return std::make_unique<linux_backend::LinuxLauncher>(config);
}

}  // namespace broapps
#endif
