#pragma once

#include "broapps/app_catalog.h"
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace broapps {

class CatalogBase : public AppCatalog {
public:
    explicit CatalogBase(CatalogConfig config);
    ~CatalogBase() override = default;

    const std::vector<AppInfo>& apps() const override;
    std::optional<AppInfo> find_by_id(std::string_view id) const override;
    std::vector<AppInfo> search(std::string_view query) const override;
    std::vector<AppInfo> find_by_category(std::string_view category) const override;
    std::vector<AppInfo> find_by_mime_type(std::string_view mime_type) const override;
    // CatalogConfig::extra_search_paths; platform catalogs add their own locations.
    std::vector<std::filesystem::path> source_directories() const override;

protected:
    void set_apps(std::vector<AppInfo> apps);
    const CatalogConfig& config() const { return config_; }

private:
    CatalogConfig config_;
    mutable std::mutex mutex_;
    std::vector<AppInfo> apps_;
};

int score_app(const AppInfo& app, const std::vector<std::string>& terms, std::string_view full_query);

}  // namespace broapps
