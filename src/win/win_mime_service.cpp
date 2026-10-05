#include "win_mime_service.h"
#include "registry_assoc.h"

#ifdef _WIN32
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <unordered_set>

namespace broapps::win_backend {

namespace {

std::string to_lower_str(std::string_view sv) {
    std::string out;
    out.reserve(sv.size());
    for (char c : sv) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

}  // namespace

WinMimeService::WinMimeService(std::shared_ptr<AppCatalog> catalog)
    : MimeServiceBase(std::move(catalog)) {}

std::optional<AppInfo> WinMimeService::get_default_app_for_mime(std::string_view mime_type) {
    if (auto app = registered_default(mime_type)) return app;

    auto cands = get_candidates_for_mime(mime_type);
    if (!cands.empty()) return cands.front();

    return std::nullopt;
}

// The registry's default handler only. get_candidates_for_mime starts from this; it must not
// fall back to the candidates itself (the two used to recurse into each other forever when a
// type had no registered default).
std::optional<AppInfo> WinMimeService::registered_default(std::string_view mime_type) const {
    auto exts = mime_to_extensions(mime_type);
    for (const auto& ext : exts) {
        auto assoc = query_registry_associations(ext);
        if (!assoc.default_executable.empty()) {
            // Check in catalog
            std::string exe_lower = to_lower_str(assoc.default_executable);
            std::string filename_lower;
            try {
                filename_lower = to_lower_str(std::filesystem::path(assoc.default_executable).filename().string());
            } catch (...) {}

            if (catalog_) {
                for (const auto& app : catalog_->apps()) {
                    std::string app_exe_lower = to_lower_str(app.executable_path);
                    if (app_exe_lower == exe_lower) {
                        return app;
                    }
                    try {
                        std::string app_fn = to_lower_str(std::filesystem::path(app.executable_path).filename().string());
                        if (!app_fn.empty() && app_fn == filename_lower) {
                            return app;
                        }
                    } catch (...) {}
                }
            }

            // Synthesize an AppInfo if not in catalog
            AppInfo app;
            app.executable_path = assoc.default_executable;
            try {
                app.name = std::filesystem::path(assoc.default_executable).stem().string();
                app.id = app.name;
            } catch (...) {
                app.name = assoc.default_executable;
                app.id = assoc.default_executable;
            }
            return app;
        }

        if (!assoc.default_progid.empty() && catalog_) {
            auto app = catalog_->find_by_id(assoc.default_progid);
            if (app) return app;
        }
    }

    return std::nullopt;
}

std::vector<AppInfo> WinMimeService::get_candidates_for_mime(std::string_view mime_type) {
    std::vector<AppInfo> results;
    std::unordered_set<std::string> seen_exes;

    auto def_app = registered_default(mime_type);
    if (def_app) {
        seen_exes.insert(to_lower_str(def_app->executable_path));
        results.push_back(std::move(*def_app));
    }

    auto exts = mime_to_extensions(mime_type);
    for (const auto& ext : exts) {
        auto assoc = query_registry_associations(ext);
        for (const auto& cand_exe : assoc.candidate_executables) {
            std::string cand_lower = to_lower_str(cand_exe);
            if (seen_exes.contains(cand_lower)) continue;

            bool found = false;
            if (catalog_) {
                for (const auto& app : catalog_->apps()) {
                    if (to_lower_str(app.executable_path) == cand_lower) {
                        seen_exes.insert(cand_lower);
                        results.push_back(app);
                        found = true;
                        break;
                    }
                }
            }

            if (!found) {
                AppInfo app;
                app.executable_path = cand_exe;
                try {
                    app.name = std::filesystem::path(cand_exe).stem().string();
                    app.id = app.name;
                } catch (...) {
                    app.name = cand_exe;
                    app.id = cand_exe;
                }
                seen_exes.insert(cand_lower);
                results.push_back(std::move(app));
            }
        }
    }

    return results;
}

}  // namespace broapps::win_backend

namespace broapps {

std::unique_ptr<MimeService> MimeService::create(std::shared_ptr<AppCatalog> catalog) {
    return std::make_unique<win_backend::WinMimeService>(std::move(catalog));
}

}  // namespace broapps
#endif
