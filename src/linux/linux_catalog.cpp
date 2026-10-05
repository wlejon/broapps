#include "linux_catalog.h"
#include "xdg_scanner.h"

namespace broapps::linux_backend {

LinuxCatalog::LinuxCatalog(CatalogConfig config)
    : CatalogBase(std::move(config)) {
    refresh();
}

void LinuxCatalog::refresh() {
    auto scanned = scan_xdg_applications(config().extra_search_paths, config().include_nodisplay);
    set_apps(std::move(scanned));
}

std::vector<std::filesystem::path> LinuxCatalog::source_directories() const {
    std::vector<std::filesystem::path> out;
    for (const auto& d : get_xdg_application_dirs(config().extra_search_paths)) out.emplace_back(d);
    return out;
}

}  // namespace broapps::linux_backend

#if defined(__linux__)
namespace broapps {

std::unique_ptr<AppCatalog> AppCatalog::create(const CatalogConfig& config) {
    return std::make_unique<linux_backend::LinuxCatalog>(config);
}

}  // namespace broapps
#endif
