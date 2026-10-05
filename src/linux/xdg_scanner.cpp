#include "xdg_scanner.h"
#include "desktop_entry.h"

#include <cstdlib>
#include <filesystem>
#include <unordered_set>

#ifndef _WIN32
#include <unistd.h>
#endif

namespace broapps::linux_backend {

namespace {

std::vector<std::string> split_path_list(std::string_view str, char delimiter = ':') {
    std::vector<std::string> out;
    size_t start = 0;
    while (start < str.size()) {
        size_t end = str.find(delimiter, start);
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

std::string derive_id_from_relpath(const std::filesystem::path& rel_path) {
    std::string id;
    for (auto it = rel_path.begin(); it != rel_path.end(); ++it) {
        if (!id.empty()) id.push_back('-');
        id.append(it->string());
    }
    return id;
}

}  // namespace

std::vector<std::string> get_xdg_application_dirs(const std::vector<std::string>& extra_paths) {
    std::vector<std::string> dirs;

    // 1. Extra paths first
    for (const auto& p : extra_paths) {
        dirs.push_back(p);
    }

    // 2. $XDG_DATA_HOME/applications
    const char* data_home = std::getenv("XDG_DATA_HOME");
    if (data_home && *data_home != '\0') {
        dirs.push_back((std::filesystem::path(data_home) / "applications").string());
    } else {
        const char* home = std::getenv("HOME");
        if (home && *home != '\0') {
            dirs.push_back((std::filesystem::path(home) / ".local" / "share" / "applications").string());
        }
    }

    // 3. $XDG_DATA_DIRS/applications
    const char* data_dirs = std::getenv("XDG_DATA_DIRS");
    if (data_dirs && *data_dirs != '\0') {
        for (const auto& d : split_path_list(data_dirs)) {
            dirs.push_back((std::filesystem::path(d) / "applications").string());
        }
    } else {
        dirs.push_back("/usr/local/share/applications");
        dirs.push_back("/usr/share/applications");
    }

    return dirs;
}

bool check_try_exec(const std::string& try_exec) {
    if (try_exec.empty()) return true;

    // If it contains a slash, it's a direct path
    if (try_exec.find('/') != std::string::npos) {
#ifndef _WIN32
        return access(try_exec.c_str(), X_OK) == 0;
#else
        std::error_code ec;
        return std::filesystem::exists(try_exec, ec);
#endif
    }

    // Otherwise search PATH
    const char* path_env = std::getenv("PATH");
    if (!path_env || *path_env == '\0') {
        path_env = "/usr/bin:/bin:/usr/local/bin";
    }

    for (const auto& dir : split_path_list(path_env)) {
        auto candidate = std::filesystem::path(dir) / try_exec;
#ifndef _WIN32
        if (access(candidate.c_str(), X_OK) == 0) {
            return true;
        }
#else
        std::error_code ec;
        if (std::filesystem::exists(candidate, ec)) {
            return true;
        }
#endif
    }

    return false;
}

std::vector<AppInfo> scan_xdg_applications(
    const std::vector<std::string>& extra_paths,
    bool include_nodisplay) {

    std::vector<AppInfo> result;
    std::unordered_set<std::string> seen_ids;

    auto search_dirs = get_xdg_application_dirs(extra_paths);

    for (const auto& dir_str : search_dirs) {
        std::error_code ec;
        std::filesystem::path base_dir(dir_str);
        if (!std::filesystem::exists(base_dir, ec) || !std::filesystem::is_directory(base_dir, ec)) {
            continue;
        }

        for (auto it = std::filesystem::recursive_directory_iterator(base_dir, std::filesystem::directory_options::skip_permission_denied, ec);
             it != std::filesystem::recursive_directory_iterator();
             it.increment(ec)) {

            if (ec) continue;
            if (it->is_regular_file(ec) && it->path().extension() == ".desktop") {
                auto rel = std::filesystem::relative(it->path(), base_dir, ec);
                std::string entry_id = derive_id_from_relpath(rel);

                if (seen_ids.contains(entry_id)) {
                    // Higher-precedence directory already provided this ID
                    continue;
                }

                auto parsed = parse_desktop_entry_file(it->path().string(), entry_id);
                if (!parsed) continue;

                seen_ids.insert(entry_id);

                if (parsed->is_nodisplay && !include_nodisplay) {
                    continue;
                }

                if (!parsed->try_exec.empty() && !check_try_exec(parsed->try_exec)) {
                    continue;
                }

                result.push_back(std::move(*parsed));
            }
        }
    }

    return result;
}

}  // namespace broapps::linux_backend
