#pragma once

#include "broapps/app_launcher.h"
#include "process_handle_base.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace broapps::linux_backend {

class LinuxProcessHandle : public ProcessHandleBase {
public:
    LinuxProcessHandle(
        uint64_t launch_id,
        int64_t pid,
        std::string scope_id,
        bool is_systemd_scope);

    ~LinuxProcessHandle() override;

    bool terminate() override;
    bool kill() override;

private:
    bool is_systemd_scope_ = false;
};

class LinuxLauncher : public AppLauncher {
public:
    explicit LinuxLauncher(LauncherConfig config);
    ~LinuxLauncher() override;

    MessageQueue<LaunchEvent>& events() override { return events_; }

    std::shared_ptr<ProcessHandle> launch(
        const AppInfo& app,
        const LaunchScope& scope = {}) override;

    std::shared_ptr<ProcessHandle> launch_executable(
        const std::string& exec_path,
        const LaunchScope& scope = {}) override;

    static bool is_systemd_user_available();

private:
    LauncherConfig config_;
    MessageQueue<LaunchEvent> events_;
    std::atomic<uint64_t> next_launch_id_{1};

    std::shared_ptr<ProcessHandle> launch_argv(
        const std::string& app_id,
        const std::vector<std::string>& argv,
        const LaunchScope& scope,
        bool is_terminal);
};

}  // namespace broapps::linux_backend
