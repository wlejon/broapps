#pragma once

#include "broapps/app_info.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace broapps {

struct CatalogConfig {
    std::vector<std::string> extra_search_paths;
    bool include_nodisplay = false;
};

class AppCatalog {
public:
    virtual ~AppCatalog() = default;

    static std::unique_ptr<AppCatalog> create(const CatalogConfig& config = {});

    virtual const std::vector<AppInfo>& apps() const = 0;
    virtual std::optional<AppInfo> find_by_id(std::string_view id) const = 0;
    virtual std::vector<AppInfo> search(std::string_view query) const = 0;
    virtual std::vector<AppInfo> find_by_category(std::string_view category) const = 0;
    virtual std::vector<AppInfo> find_by_mime_type(std::string_view mime_type) const = 0;
    virtual void refresh() = 0;

    // The directories the catalog is scanned from: the platform's application locations (Start
    // Menu Programs folders, XDG applications dirs, Applications folders) and
    // CatalogConfig::extra_search_paths. Some may not exist. CatalogWatcher watches these.
    virtual std::vector<std::filesystem::path> source_directories() const = 0;
};

}  // namespace broapps
