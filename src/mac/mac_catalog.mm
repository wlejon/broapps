#include "mac_catalog.h"
#include "mac_bundle.h"

#if defined(__APPLE__)
namespace broapps::mac_backend {

MacCatalog::MacCatalog(CatalogConfig config)
    : CatalogBase(std::move(config)) {
    refresh();
}

void MacCatalog::refresh() {
    auto scanned = scan_mac_applications(config().extra_search_paths, config().include_nodisplay);
    set_apps(std::move(scanned));
}

std::vector<std::filesystem::path> MacCatalog::source_directories() const {
    std::vector<std::filesystem::path> out;
    for (const auto& d : get_mac_application_dirs(config().extra_search_paths)) out.emplace_back(d);
    return out;
}

}  // namespace broapps::mac_backend

namespace broapps {

std::unique_ptr<AppCatalog> AppCatalog::create(const CatalogConfig& config) {
    return std::make_unique<mac_backend::MacCatalog>(config);
}

}  // namespace broapps
#endif
