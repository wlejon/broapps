#pragma once

#if defined(__APPLE__)
#include "broapps/app_launcher.h"
#include "process_handle_base.h"

#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace broapps::mac_backend {

class MacProcessHandle : public ProcessHandleBase {
public:
    MacProcessHandle(
        uint64_t launch_id,
        int64_t pid,
        std::string scope_id);

    ~MacProcessHandle() override;

    bool terminate() override;
    bool kill() override;
};

class MacLauncher : public AppLauncher {
public:
    explicit MacLauncher(LauncherConfig config);
    ~MacLauncher() override;

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

    std::shared_ptr<ProcessHandle> launch_argv(
        const std::string& app_id,
        const std::vector<std::string>& argv,
        const LaunchScope& scope);
};

}  // namespace broapps::mac_backend
#endif
