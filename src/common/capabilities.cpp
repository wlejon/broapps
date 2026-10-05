#include "broapps/capabilities.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__linux__)
#include "src/linux/linux_launcher.h"
#elif defined(__APPLE__)
// macOS
#endif

namespace broapps {

LauncherCapabilities query_launcher_capabilities() {
    LauncherCapabilities caps;
#if defined(_WIN32)
    caps.has_scoped_isolation = true;
    caps.has_packaged_app_launch = true;
    caps.has_terminal_launch = true;
    caps.has_job_object_kill = true;
    caps.has_systemd_cgroup = false;
#elif defined(__linux__)
    caps.has_systemd_cgroup = linux_backend::LinuxLauncher::is_systemd_user_available();
    caps.has_scoped_isolation = caps.has_systemd_cgroup;
    caps.has_packaged_app_launch = false;
    caps.has_terminal_launch = true;
    caps.has_job_object_kill = false;
#elif defined(__APPLE__)
    caps.has_scoped_isolation = false;
    caps.has_packaged_app_launch = true;
    caps.has_terminal_launch = true;
    caps.has_job_object_kill = false;
    caps.has_systemd_cgroup = false;
#endif
    return caps;
}

CatalogCapabilities query_catalog_capabilities() {
    CatalogCapabilities caps;
#if defined(_WIN32)
    caps.supports_categories = false;
    caps.supports_keywords = false;
    caps.supports_mime_types = false;
    caps.supports_actions = false;
    caps.supports_packaged_apps = true;
#elif defined(__linux__)
    caps.supports_categories = true;
    caps.supports_keywords = true;
    caps.supports_mime_types = true;
    caps.supports_actions = true;
    caps.supports_packaged_apps = false;
#elif defined(__APPLE__)
    caps.supports_categories = true;
    caps.supports_keywords = true;
    caps.supports_mime_types = true;
    caps.supports_actions = false;
    caps.supports_packaged_apps = true;
#endif
    return caps;
}

}  // namespace broapps
