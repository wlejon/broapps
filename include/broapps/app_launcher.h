#pragma once

#include "broapps/app_info.h"
#include "broapps/event_queue.h"
#include "broapps/launch_event.h"
#include "broapps/launch_scope.h"
#include "broapps/process_handle.h"

#include <memory>
#include <string>

namespace broapps {

struct LauncherConfig {
    bool prefer_systemd_scope = true; // On Linux
    bool use_job_objects = true;      // On Windows
};

class AppLauncher {
public:
    virtual ~AppLauncher() = default;

    static std::unique_ptr<AppLauncher> create(const LauncherConfig& config = {});

    virtual MessageQueue<LaunchEvent>& events() = 0;

    virtual std::shared_ptr<ProcessHandle> launch(const AppInfo& app, const LaunchScope& scope = {}) = 0;
    virtual std::shared_ptr<ProcessHandle> launch_executable(const std::string& exec_path, const LaunchScope& scope = {}) = 0;
};

}  // namespace broapps
