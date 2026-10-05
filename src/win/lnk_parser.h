#pragma once

#ifdef _WIN32
#include "broapps/app_info.h"
#include <optional>
#include <string>
#include <vector>

namespace broapps::win_backend {

// Parses a single Windows shortcut (.lnk) file
std::optional<AppInfo> parse_lnk_file(const std::wstring& lnk_path);

// Returns standard Start Menu directories for programs
std::vector<std::wstring> get_start_menu_dirs(const std::vector<std::string>& extra_paths = {});

// Scans Start Menu directories for .lnk shortcuts
std::vector<AppInfo> scan_start_menu_shortcuts(
    const std::vector<std::string>& extra_paths = {},
    bool include_nodisplay = false);

}  // namespace broapps::win_backend
#endif
