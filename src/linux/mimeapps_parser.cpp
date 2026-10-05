#include "mimeapps_parser.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace broapps::linux_backend {

namespace {

std::string trim(std::string_view sv) {
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front()))) {
        sv.remove_prefix(1);
    }
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back()))) {
        sv.remove_suffix(1);
    }
    return std::string(sv);
}

std::vector<std::string> split_semicolon_list(std::string_view val) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start < val.size()) {
        size_t end = val.find(';', start);
        if (end == std::string::npos) {
            std::string item = trim(val.substr(start));
            if (!item.empty()) out.push_back(std::move(item));
            break;
        }
        std::string item = trim(val.substr(start, end - start));
        if (!item.empty()) out.push_back(std::move(item));
        start = end + 1;
    }
    return out;
}

std::vector<std::string> split_colon_list(std::string_view str) {
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

void parse_mimeapps_file(const std::string& file_path, MimeAssociations& assocs) {
    std::ifstream file(file_path);
    if (!file.is_open()) return;

    enum class Section { None, Default, Added, Removed };
    Section current_section = Section::None;

    std::string line;
    while (std::getline(file, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed.front() == '#') continue;

        if (trimmed.front() == '[' && trimmed.back() == ']') {
            std::string_view sec = std::string_view(trimmed).substr(1, trimmed.size() - 2);
            if (sec == "Default Applications") current_section = Section::Default;
            else if (sec == "Added Associations") current_section = Section::Added;
            else if (sec == "Removed Associations") current_section = Section::Removed;
            else current_section = Section::None;
            continue;
        }

        auto eq_pos = trimmed.find('=');
        if (eq_pos == std::string::npos) continue;

        std::string mime_type = trim(trimmed.substr(0, eq_pos));
        std::string val = trimmed.substr(eq_pos + 1);
        auto desktop_ids = split_semicolon_list(val);

        if (current_section == Section::Default) {
            if (!desktop_ids.empty()) {
                // Higher-precedence files are loaded first, so keep first seen
                if (!assocs.defaults.contains(mime_type)) {
                    assocs.defaults[mime_type] = desktop_ids.front();
                }
                for (const auto& id : desktop_ids) {
                    auto& cands = assocs.candidates[mime_type];
                    if (std::find(cands.begin(), cands.end(), id) == cands.end()) {
                        cands.push_back(id);
                    }
                }
            }
        } else if (current_section == Section::Added) {
            auto& cands = assocs.candidates[mime_type];
            for (const auto& id : desktop_ids) {
                if (std::find(cands.begin(), cands.end(), id) == cands.end()) {
                    cands.push_back(id);
                }
            }
        } else if (current_section == Section::Removed) {
            auto it = assocs.candidates.find(mime_type);
            if (it != assocs.candidates.end()) {
                for (const auto& id : desktop_ids) {
                    it->second.erase(std::remove(it->second.begin(), it->second.end(), id), it->second.end());
                }
            }
        }
    }
}

void parse_mimeinfo_cache_file(const std::string& file_path, MimeAssociations& assocs) {
    std::ifstream file(file_path);
    if (!file.is_open()) return;

    std::string line;
    bool in_mime_cache = false;

    while (std::getline(file, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed.front() == '#') continue;

        if (trimmed == "[MIME Cache]") {
            in_mime_cache = true;
            continue;
        } else if (trimmed.front() == '[' && trimmed.back() == ']') {
            in_mime_cache = false;
            continue;
        }

        if (!in_mime_cache) continue;

        auto eq_pos = trimmed.find('=');
        if (eq_pos == std::string::npos) continue;

        std::string mime_type = trim(trimmed.substr(0, eq_pos));
        std::string val = trimmed.substr(eq_pos + 1);
        auto desktop_ids = split_semicolon_list(val);

        auto& cands = assocs.candidates[mime_type];
        for (const auto& id : desktop_ids) {
            if (std::find(cands.begin(), cands.end(), id) == cands.end()) {
                cands.push_back(id);
            }
        }
    }
}

MimeAssociations load_system_mime_associations() {
    MimeAssociations assocs;

    // Freedesktop specification search hierarchy (highest precedence first)
    std::vector<std::string> mimeapps_files;

    // 1. $XDG_CONFIG_HOME/mimeapps.list
    const char* config_home = std::getenv("XDG_CONFIG_HOME");
    const char* home = std::getenv("HOME");

    if (config_home && *config_home != '\0') {
        mimeapps_files.push_back((std::filesystem::path(config_home) / "mimeapps.list").string());
    } else if (home && *home != '\0') {
        mimeapps_files.push_back((std::filesystem::path(home) / ".config" / "mimeapps.list").string());
    }

    // 2. /etc/xdg/mimeapps.list
    const char* config_dirs = std::getenv("XDG_CONFIG_DIRS");
    if (config_dirs && *config_dirs != '\0') {
        for (const auto& d : split_colon_list(config_dirs)) {
            mimeapps_files.push_back((std::filesystem::path(d) / "mimeapps.list").string());
        }
    } else {
        mimeapps_files.push_back("/etc/xdg/mimeapps.list");
    }

    // 3. $XDG_DATA_HOME/applications/mimeapps.list
    const char* data_home = std::getenv("XDG_DATA_HOME");
    if (data_home && *data_home != '\0') {
        mimeapps_files.push_back((std::filesystem::path(data_home) / "applications" / "mimeapps.list").string());
        mimeapps_files.push_back((std::filesystem::path(data_home) / "applications" / "defaults.list").string());
    } else if (home && *home != '\0') {
        mimeapps_files.push_back((std::filesystem::path(home) / ".local" / "share" / "applications" / "mimeapps.list").string());
        mimeapps_files.push_back((std::filesystem::path(home) / ".local" / "share" / "applications" / "defaults.list").string());
    }

    // 4. $XDG_DATA_DIRS/applications/mimeapps.list
    std::vector<std::string> data_dirs;
    const char* data_dirs_env = std::getenv("XDG_DATA_DIRS");
    if (data_dirs_env && *data_dirs_env != '\0') {
        data_dirs = split_colon_list(data_dirs_env);
    } else {
        data_dirs = {"/usr/local/share", "/usr/share"};
    }

    for (const auto& d : data_dirs) {
        mimeapps_files.push_back((std::filesystem::path(d) / "applications" / "mimeapps.list").string());
        mimeapps_files.push_back((std::filesystem::path(d) / "applications" / "defaults.list").string());
    }

    // Parse mimeapps.list files in precedence order
    for (const auto& f : mimeapps_files) {
        std::error_code ec;
        if (std::filesystem::exists(f, ec)) {
            parse_mimeapps_file(f, assocs);
        }
    }

    // Also parse mimeinfo.cache in data dirs
    if (data_home && *data_home != '\0') {
        auto p = std::filesystem::path(data_home) / "applications" / "mimeinfo.cache";
        if (std::filesystem::exists(p)) parse_mimeinfo_cache_file(p.string(), assocs);
    } else if (home && *home != '\0') {
        auto p = std::filesystem::path(home) / ".local" / "share" / "applications" / "mimeinfo.cache";
        if (std::filesystem::exists(p)) parse_mimeinfo_cache_file(p.string(), assocs);
    }

    for (const auto& d : data_dirs) {
        auto p = std::filesystem::path(d) / "applications" / "mimeinfo.cache";
        if (std::filesystem::exists(p)) parse_mimeinfo_cache_file(p.string(), assocs);
    }

    return assocs;
}

}  // namespace broapps::linux_backend
