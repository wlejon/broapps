#include "linux_mime_service.h"
#include <algorithm>
#include <unordered_set>

namespace broapps::linux_backend {

LinuxMimeService::LinuxMimeService(std::shared_ptr<AppCatalog> catalog)
    : MimeServiceBase(std::move(catalog)),
      assocs_(load_system_mime_associations()) {}

bool LinuxMimeService::set_default_app_for_mime(std::string_view mime_type, std::string_view app_id) {
    if (mime_type.empty() || app_id.empty()) return false;

    assocs_.defaults[std::string(mime_type)] = std::string(app_id);

    auto& cands = assocs_.candidates[std::string(mime_type)];
    auto it = std::find(cands.begin(), cands.end(), app_id);
    if (it != cands.end()) {
        cands.erase(it);
    }
    cands.insert(cands.begin(), std::string(app_id));

    return save_default_mime_association(mime_type, app_id);
}

std::optional<AppInfo> LinuxMimeService::get_default_app_for_mime(std::string_view mime_type) {
    auto it = assocs_.defaults.find(std::string(mime_type));
    if (it != assocs_.defaults.end() && !it->second.empty()) {
        auto app = find_app_in_catalog(it->second);
        if (app) return app;

        AppInfo fallback;
        fallback.id = it->second;
        fallback.name = it->second;
        return fallback;
    }

    auto cands = get_candidates_for_mime(mime_type);
    if (!cands.empty()) return cands.front();

    return std::nullopt;
}

std::vector<AppInfo> LinuxMimeService::get_candidates_for_mime(std::string_view mime_type) {
    std::vector<AppInfo> result;
    std::unordered_set<std::string> seen_ids;

    // 1. Check default first
    auto it_def = assocs_.defaults.find(std::string(mime_type));
    if (it_def != assocs_.defaults.end() && !it_def->second.empty()) {
        auto app = find_app_in_catalog(it_def->second);
        if (app) {
            seen_ids.insert(app->id);
            result.push_back(std::move(*app));
        } else {
            AppInfo fallback;
            fallback.id = it_def->second;
            fallback.name = it_def->second;
            seen_ids.insert(fallback.id);
            result.push_back(std::move(fallback));
        }
    }

    // 2. Add candidates from mimeapps associations
    auto it = assocs_.candidates.find(std::string(mime_type));
    if (it != assocs_.candidates.end()) {
        for (const auto& desktop_id : it->second) {
            auto app = find_app_in_catalog(desktop_id);
            if (app && !seen_ids.contains(app->id)) {
                seen_ids.insert(app->id);
                result.push_back(std::move(*app));
            }
        }
    }

    // 3. Add any apps in catalog declaring support for this MIME type
    for (const auto& app : filter_catalog_by_mime(mime_type)) {
        if (!seen_ids.contains(app.id)) {
            seen_ids.insert(app.id);
            result.push_back(app);
        }
    }

    return result;
}

}  // namespace broapps::linux_backend

#if defined(__linux__)
namespace broapps {

std::unique_ptr<MimeService> MimeService::create(std::shared_ptr<AppCatalog> catalog) {
    return std::make_unique<linux_backend::LinuxMimeService>(std::move(catalog));
}

}  // namespace broapps
#endif
