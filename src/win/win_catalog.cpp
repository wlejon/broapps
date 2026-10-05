#include "win_catalog.h"
#include "apps_folder.h"
#include "lnk_parser.h"

#ifdef _WIN32
#include <algorithm>
#include <unordered_map>
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

WinCatalog::WinCatalog(CatalogConfig config)
    : CatalogBase(std::move(config)) {
    refresh();
}

void WinCatalog::refresh() {
    auto shortcuts = scan_start_menu_shortcuts(config().extra_search_paths, config().include_nodisplay);
    auto apps_folder = enumerate_apps_folder(config().include_nodisplay);

    std::vector<AppInfo> merged;
    std::unordered_map<std::string, size_t> by_name_lower;
    std::unordered_map<std::string, size_t> by_exe_lower;
    std::unordered_set<std::string> seen_ids;

    // 1. Add Start Menu shortcuts
    for (auto& s : shortcuts) {
        if (s.id.empty() || seen_ids.contains(s.id)) continue;
        seen_ids.insert(s.id);

        size_t idx = merged.size();
        if (!s.name.empty()) {
            by_name_lower[to_lower_str(s.name)] = idx;
        }
        if (!s.executable_path.empty()) {
            by_exe_lower[to_lower_str(s.executable_path)] = idx;
        }
        merged.push_back(std::move(s));
    }

    // 2. Merge AppsFolder entries
    for (auto& af : apps_folder) {
        // Check if matching an existing shortcut by name or exe
        std::string name_key = to_lower_str(af.name);
        std::string exe_key = to_lower_str(af.executable_path);

        size_t match_idx = size_t(-1);
        if (!exe_key.empty() && by_exe_lower.contains(exe_key)) {
            match_idx = by_exe_lower[exe_key];
        } else if (!name_key.empty() && by_name_lower.contains(name_key)) {
            match_idx = by_name_lower[name_key];
        }

        if (match_idx != size_t(-1)) {
            // Enrich existing entry with AUMID and packaged status
            if (!af.aumid.empty()) {
                merged[match_idx].aumid = af.aumid;
            }
            if (af.is_packaged) {
                merged[match_idx].is_packaged = true;
            }
            if (merged[match_idx].comment.empty() && !af.comment.empty()) {
                merged[match_idx].comment = af.comment;
            }
        } else {
            // New app from AppsFolder (e.g. packaged Store app)
            if (seen_ids.contains(af.id)) continue;
            seen_ids.insert(af.id);

            size_t idx = merged.size();
            if (!af.name.empty()) by_name_lower[name_key] = idx;
            if (!af.executable_path.empty()) by_exe_lower[exe_key] = idx;

            merged.push_back(std::move(af));
        }
    }

    set_apps(std::move(merged));
}

std::vector<std::filesystem::path> WinCatalog::source_directories() const {
    std::vector<std::filesystem::path> out;
    for (const auto& d : get_start_menu_dirs(config().extra_search_paths)) out.emplace_back(d);
    return out;
}

}  // namespace broapps::win_backend

namespace broapps {

std::unique_ptr<AppCatalog> AppCatalog::create(const CatalogConfig& config) {
    return std::make_unique<win_backend::WinCatalog>(config);
}

}  // namespace broapps
#endif
