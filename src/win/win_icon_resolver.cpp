#include "win_icon_resolver.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <filesystem>

namespace broapps::win_backend {

std::optional<std::filesystem::path> WinIconResolver::resolve_icon(
    const std::string& icon_name_or_path,
    uint32_t preferred_size) {

    (void)preferred_size;
    if (icon_name_or_path.empty()) return std::nullopt;

    std::string path_str = icon_name_or_path;
    // Strip trailing icon index e.g. ",0"
    auto comma_pos = path_str.rfind(',');
    if (comma_pos != std::string::npos && comma_pos > 0) {
        path_str = path_str.substr(0, comma_pos);
    }

    std::error_code ec;
    std::filesystem::path p(path_str);
    if (std::filesystem::exists(p, ec)) {
        return p;
    }

    // Try resolving in Windows directory if relative
    char win_dir[MAX_PATH] = {};
    if (GetWindowsDirectoryA(win_dir, MAX_PATH)) {
        auto cand1 = std::filesystem::path(win_dir) / "System32" / path_str;
        if (std::filesystem::exists(cand1, ec)) return cand1;
    }

    return std::nullopt;
}

}  // namespace broapps::win_backend

namespace broapps {

std::unique_ptr<IconResolver> IconResolver::create() {
    return std::make_unique<win_backend::WinIconResolver>();
}

}  // namespace broapps
#endif
