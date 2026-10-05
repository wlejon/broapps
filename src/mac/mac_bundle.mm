#include "mac_bundle.h"
#include "plist_parser.h"

#if defined(__APPLE__)
#include <cstdlib>
#include <filesystem>
#include <unordered_set>

namespace broapps::mac_backend {

std::vector<std::string> get_mac_application_dirs(const std::vector<std::string>& extra_paths) {
    std::vector<std::string> dirs;

    for (const auto& ep : extra_paths) {
        dirs.push_back(ep);
    }

    const char* home = std::getenv("HOME");
    if (home) {
        dirs.push_back((std::filesystem::path(home) / "Applications").string());
    }

    dirs.push_back("/Applications");
    dirs.push_back("/System/Applications");
    dirs.push_back("/System/Library/CoreServices/Applications");

    return dirs;
}

std::vector<AppInfo> scan_mac_applications(
    const std::vector<std::string>& extra_paths,
    bool include_nodisplay) {

    std::vector<AppInfo> results;
    std::unordered_set<std::string> seen_ids;
    auto dirs = get_mac_application_dirs(extra_paths);

    for (const auto& dir : dirs) {
        std::error_code ec;
        std::filesystem::path base_p(dir);
        if (!std::filesystem::exists(base_p, ec) || !std::filesystem::is_directory(base_p, ec)) {
            continue;
        }

        // We scan up to depth 3 so /Applications/Utilities/Terminal.app is found
        for (auto it = std::filesystem::recursive_directory_iterator(base_p, std::filesystem::directory_options::skip_permission_denied, ec);
             it != std::filesystem::recursive_directory_iterator();
             it.increment(ec)) {

            if (ec) continue;

            if (it->is_directory(ec) && it->path().extension() == ".app") {
                auto app = parse_bundle_info_plist(it->path().string());
                it.disable_recursion_pending(); // Do not recurse inside .app bundle

                if (!app) continue;
                if (seen_ids.contains(app->id)) continue;

                if (app->is_nodisplay && !include_nodisplay) {
                    continue;
                }

                seen_ids.insert(app->id);
                results.push_back(std::move(*app));
            } else if (it.depth() >= 2) {
                it.disable_recursion_pending();
            }
        }
    }

    return results;
}

}  // namespace broapps::mac_backend
#endif
