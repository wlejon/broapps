#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace broapps::linux_backend {

struct MimeAssociations {
    // Maps mime_type -> default desktop file id
    std::unordered_map<std::string, std::string> defaults;

    // Maps mime_type -> ordered candidate desktop file ids
    std::unordered_map<std::string, std::vector<std::string>> candidates;
};

// Parses a single mimeapps.list or defaults.list file
void parse_mimeapps_file(const std::string& file_path, MimeAssociations& assocs);

// Parses a mimeinfo.cache file
void parse_mimeinfo_cache_file(const std::string& file_path, MimeAssociations& assocs);

// Loads all MIME associations according to the Freedesktop specification search hierarchy
MimeAssociations load_system_mime_associations();

}  // namespace broapps::linux_backend
