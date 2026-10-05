#include "linux_icon_resolver.h"

#include <cstdlib>
#include <filesystem>
#include <sstream>

namespace broapps::linux_backend {

namespace {

std::vector<std::string> split_colon(std::string_view str) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start < str.size()) {
        size_t end = str.find(':', start);
        if (end == std::string::npos) {
            std::string item = std::string(str.substr(start));
            if (!item.empty()) out.push_back(std::move(item));
            break;
        }
        std::string item = std::string(str.substr(start, end - start));
        if (!item.empty()) out.push_back(std::move(item));
        start = end + 1;
    }
    return out;
}

}  // namespace

LinuxIconResolver::LinuxIconResolver() {
    const char* data_home = std::getenv("XDG_DATA_HOME");
    const char* home = std::getenv("HOME");

    if (data_home && *data_home != '\0') {
        icon_search_dirs_.push_back((std::filesystem::path(data_home) / "icons").string());
    } else if (home && *home != '\0') {
        icon_search_dirs_.push_back((std::filesystem::path(home) / ".local" / "share" / "icons").string());
        icon_search_dirs_.push_back((std::filesystem::path(home) / ".icons").string());
    }

    const char* data_dirs = std::getenv("XDG_DATA_DIRS");
    if (data_dirs && *data_dirs != '\0') {
        for (const auto& d : split_colon(data_dirs)) {
            icon_search_dirs_.push_back((std::filesystem::path(d) / "icons").string());
        }
    } else {
        icon_search_dirs_.push_back("/usr/local/share/icons");
        icon_search_dirs_.push_back("/usr/share/icons");
    }

    icon_search_dirs_.push_back("/usr/share/pixmaps");
}

std::optional<std::filesystem::path> LinuxIconResolver::resolve_icon(
    const std::string& icon_name_or_path,
    uint32_t preferred_size) {

    if (icon_name_or_path.empty()) return std::nullopt;

    std::error_code ec;

    // 1. Direct path check
    if (icon_name_or_path.front() == '/' || icon_name_or_path.starts_with("./")) {
        std::filesystem::path p(icon_name_or_path);
        if (std::filesystem::exists(p, ec) && std::filesystem::is_regular_file(p, ec)) {
            return p;
        }
    }

    // Extensions to check
    static const char* extensions[] = {".png", ".svg", ".xpm"};

    // Common icon themes
    static const char* themes[] = {"hicolor", "breeze", "Adwaita", "gnome"};

    // Sizes to test around preferred_size
    std::string size_str = std::to_string(preferred_size) + "x" + std::to_string(preferred_size);
    std::vector<std::string> size_subdirs = {
        size_str + "/apps",
        "scalable/apps",
        "48x48/apps",
        "64x64/apps",
        "32x32/apps",
        "128x128/apps",
        "256x256/apps",
        "apps"
    };

    // 2. Search in icon search dirs with themes
    for (const auto& base_dir : icon_search_dirs_) {
        std::filesystem::path bp(base_dir);
        if (!std::filesystem::exists(bp, ec)) continue;

        // Try direct in base_dir (e.g. /usr/share/pixmaps/app.png)
        for (const char* ext : extensions) {
            std::filesystem::path cand = bp / (icon_name_or_path + ext);
            if (std::filesystem::exists(cand, ec)) return cand;
        }
        std::filesystem::path direct = bp / icon_name_or_path;
        if (std::filesystem::exists(direct, ec)) return direct;

        // Try themes
        for (const char* theme : themes) {
            std::filesystem::path tp = bp / theme;
            if (!std::filesystem::exists(tp, ec)) continue;

            for (const auto& sz : size_subdirs) {
                std::filesystem::path sp = tp / sz;
                if (!std::filesystem::exists(sp, ec)) continue;

                for (const char* ext : extensions) {
                    std::filesystem::path cand = sp / (icon_name_or_path + ext);
                    if (std::filesystem::exists(cand, ec)) return cand;
                }
                std::filesystem::path cand_direct = sp / icon_name_or_path;
                if (std::filesystem::exists(cand_direct, ec)) return cand_direct;
            }
        }
    }

    return std::nullopt;
}

}  // namespace broapps::linux_backend

#if defined(__linux__)
namespace broapps {

std::unique_ptr<IconResolver> IconResolver::create() {
    return std::make_unique<linux_backend::LinuxIconResolver>();
}

}  // namespace broapps
#endif
