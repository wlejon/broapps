#pragma once

#ifdef _WIN32
#include "broapps/app_launcher.h"
#include "process_handle_base.h"
#include "job_object.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace broapps::win_backend {

class WinProcessHandle : public ProcessHandleBase {
public:
    WinProcessHandle(
        uint64_t launch_id,
        int64_t pid,
        std::string scope_id,
        HANDLE hProcess,
        HANDLE hThread,
        JobObject job);

    ~WinProcessHandle() override;

    bool terminate() override;
    bool kill() override;

private:
    HANDLE hProcess_ = nullptr;
    HANDLE hThread_ = nullptr;
    JobObject job_;
};

class WinLauncher : public AppLauncher {
public:
    explicit WinLauncher(LauncherConfig config);
    ~WinLauncher() override;

    MessageQueue<LaunchEvent>& events() override { return events_; }

    std::shared_ptr<ProcessHandle> launch(
        const AppInfo& app,
        const LaunchScope& scope = {}) override;

    std::shared_ptr<ProcessHandle> launch_executable(
        const std::string& exec_path,
        const LaunchScope& scope = {}) override;

private:
    LauncherConfig config_;
    MessageQueue<LaunchEvent> events_;
    std::atomic<uint64_t> next_launch_id_{1};

    std::shared_ptr<ProcessHandle> launch_packaged(
        const AppInfo& app,
        const LaunchScope& scope,
        uint64_t lid);

    std::shared_ptr<ProcessHandle> launch_win32(
        const std::string& app_id,
        const std::string& exec_path,
        const std::string& existing_args,
        const LaunchScope& scope,
        uint64_t lid);
};

}  // namespace broapps::win_backend
#endif
