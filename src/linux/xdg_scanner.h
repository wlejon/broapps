#pragma once

#include "broapps/app_info.h"
#include <string>
#include <vector>

namespace broapps::linux_backend {

// Returns the list of XDG applications directories in precedence order
std::vector<std::string> get_xdg_application_dirs(const std::vector<std::string>& extra_paths = {});

// Checks whether an executable named in TryExec exists in PATH or at the specified path
bool check_try_exec(const std::string& try_exec);

// Scans all XDG applications directories, parsing and deduplicating apps
std::vector<AppInfo> scan_xdg_applications(
    const std::vector<std::string>& extra_paths = {},
    bool include_nodisplay = false);

}  // namespace broapps::linux_backend
