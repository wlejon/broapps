#pragma once

#if defined(__APPLE__)
#include "broapps/app_info.h"
#include <optional>
#include <string>

namespace broapps::mac_backend {

// Parses Info.plist from a .app bundle directory
std::optional<AppInfo> parse_bundle_info_plist(const std::string& app_bundle_path);

}  // namespace broapps::mac_backend
#endif
