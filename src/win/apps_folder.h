#pragma once

#ifdef _WIN32
#include "broapps/app_info.h"
#include <string>
#include <vector>

namespace broapps::win_backend {

// Enumerates apps from shell:AppsFolder (captures packaged apps / MSIX / UWP / AUMIDs)
std::vector<AppInfo> enumerate_apps_folder(bool include_nodisplay = false);

}  // namespace broapps::win_backend
#endif
