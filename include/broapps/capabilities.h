#pragma once

namespace broapps {

struct LauncherCapabilities {
    bool has_scoped_isolation = false;
    bool has_packaged_app_launch = false;
    bool has_terminal_launch = false;
    bool has_job_object_kill = false;
    bool has_systemd_cgroup = false;
};

struct CatalogCapabilities {
    bool supports_categories = false;
    bool supports_keywords = false;
    bool supports_mime_types = false;
    bool supports_actions = false;
    bool supports_packaged_apps = false;
};

LauncherCapabilities query_launcher_capabilities();
CatalogCapabilities query_catalog_capabilities();

}  // namespace broapps
