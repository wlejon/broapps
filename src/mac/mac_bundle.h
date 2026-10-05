#pragma once

#if defined(__APPLE__)
#include "broapps/app_info.h"
#include <string>
#include <vector>

namespace broapps::mac_backend {

std::vector<std::string> get_mac_application_dirs(const std::vector<std::string>& extra_paths = {});

std::vector<AppInfo> scan_mac_applications(
    const std::vector<std::string>& extra_paths = {},
    bool include_nodisplay = false);

}  // namespace broapps::mac_backend
#endif
