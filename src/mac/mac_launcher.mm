#include "mac_launcher.h"

#if defined(__APPLE__)
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <spawn.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

extern char** environ;

namespace broapps::mac_backend {

MacProcessHandle::MacProcessHandle(
    uint64_t launch_id,
    int64_t pid,
    std::string scope_id)
    : ProcessHandleBase(launch_id, pid, std::move(scope_id)) {}

MacProcessHandle::~MacProcessHandle() = default;

bool MacProcessHandle::terminate() {
    int64_t p = pid_.load();
    if (p <= 0 || !is_running_.load()) return false;
    ::kill(-static_cast<pid_t>(p), SIGTERM);
    ::kill(static_cast<pid_t>(p), SIGTERM);
    return true;
}

bool MacProcessHandle::kill() {
    int64_t p = pid_.load();
    if (p <= 0 || !is_running_.load()) return false;
    ::kill(-static_cast<pid_t>(p), SIGKILL);
    ::kill(static_cast<pid_t>(p), SIGKILL);
    return true;
}

MacLauncher::MacLauncher(LauncherConfig config)
    : config_(config) {}

MacLauncher::~MacLauncher() = default;

std::shared_ptr<ProcessHandle> MacLauncher::launch(
    const AppInfo& app,
    const LaunchScope& scope) {

    std::vector<std::string> argv;

    if (!app.executable_path.empty()) {
        argv.push_back(app.executable_path);
        for (const auto& a : scope.arguments) {
            argv.push_back(a);
        }
        for (const auto& f : scope.files_to_open) {
            argv.push_back(f);
        }
    } else if (!app.bundle_id.empty()) {
        argv = {"/usr/bin/open", "-b", app.bundle_id};
        if (!scope.arguments.empty() || !scope.files_to_open.empty()) {
            argv.push_back("--args");
            for (const auto& a : scope.arguments) argv.push_back(a);
            for (const auto& f : scope.files_to_open) argv.push_back(f);
        }
    } else if (!app.bundle_path.empty()) {
        argv = {"/usr/bin/open", app.bundle_path};
        if (!scope.arguments.empty() || !scope.files_to_open.empty()) {
            argv.push_back("--args");
            for (const auto& a : scope.arguments) argv.push_back(a);
            for (const auto& f : scope.files_to_open) argv.push_back(f);
        }
    }

    if (argv.empty()) {
        uint64_t lid = next_launch_id_++;
        events_.push(AppFailed{lid, app.id, "No executable or bundle ID specified", std::chrono::system_clock::now()});
        auto h = std::make_shared<MacProcessHandle>(lid, 0, "");
        h->notify_exited(127);
        return h;
    }

    return launch_argv(app.id, argv, scope);
}

std::shared_ptr<ProcessHandle> MacLauncher::launch_executable(
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

    return launch_argv(app_id, argv, scope);
}

std::shared_ptr<ProcessHandle> MacLauncher::launch_argv(
    const std::string& app_id,
    const std::vector<std::string>& argv,
    const LaunchScope& scope) {

    uint64_t lid = next_launch_id_++;

    if (argv.empty()) {
        events_.push(AppFailed{lid, app_id, "Empty command line", std::chrono::system_clock::now()});
        auto h = std::make_shared<MacProcessHandle>(lid, 0, "");
        h->notify_exited(127);
        return h;
    }

    // Set up pipe to detect exec failures in fork/exec
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        events_.push(AppFailed{lid, app_id, std::strerror(errno), std::chrono::system_clock::now()});
        auto h = std::make_shared<MacProcessHandle>(lid, 0, "");
        h->notify_exited(127);
        return h;
    }
    fcntl(pipefd[0], F_SETFD, FD_CLOEXEC);
    fcntl(pipefd[1], F_SETFD, FD_CLOEXEC);

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        events_.push(AppFailed{lid, app_id, std::strerror(errno), std::chrono::system_clock::now()});
        auto h = std::make_shared<MacProcessHandle>(lid, 0, "");
        h->notify_exited(127);
        return h;
    }

    if (pid == 0) {
        // Child
        close(pipefd[0]);
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

        if (!scope.working_directory.empty()) {
            chdir(scope.working_directory.c_str());
        }

        for (const auto& [k, v] : scope.environment) {
            setenv(k.c_str(), v.c_str(), 1);
        }

        std::vector<char*> c_argv;
        c_argv.reserve(argv.size() + 1);
        for (const auto& s : argv) {
            c_argv.push_back(const_cast<char*>(s.data()));
        }
        c_argv.push_back(nullptr);

        execvp(c_argv[0], c_argv.data());

        int err = errno;
        write(pipefd[1], &err, sizeof(err));
        close(pipefd[1]);
        _exit(127);
    }

    // Parent
    close(pipefd[1]);
    int child_err = 0;
    ssize_t bytes_read = read(pipefd[0], &child_err, sizeof(child_err));
    close(pipefd[0]);

    if (bytes_read > 0) {
        waitpid(pid, nullptr, 0);
        std::string err_msg = "exec failed: ";
        err_msg += std::strerror(child_err);
        events_.push(AppFailed{lid, app_id, err_msg, std::chrono::system_clock::now()});
        auto h = std::make_shared<MacProcessHandle>(lid, 0, "");
        h->notify_exited(127);
        return h;
    }

    std::string scope_id = "mac-" + std::to_string(pid);
    auto handle = std::make_shared<MacProcessHandle>(lid, pid, scope_id);

    events_.push(AppStarted{
        lid,
        app_id,
        static_cast<int64_t>(pid),
        scope_id,
        std::chrono::system_clock::now()
    });

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
}

}  // namespace broapps::mac_backend

namespace broapps {

std::unique_ptr<AppLauncher> AppLauncher::create(const LauncherConfig& config) {
    return std::make_unique<mac_backend::MacLauncher>(config);
}

}  // namespace broapps
#endif
