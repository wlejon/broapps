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

// Returns the target file path for the user's mimeapps.list ($XDG_CONFIG_HOME/mimeapps.list or ~/.config/mimeapps.list)
std::string get_user_mimeapps_path();

// Updates or adds mime_type=app_id under [Default Applications] in the specified file
bool update_mimeapps_default(const std::string& file_path, std::string_view mime_type, std::string_view app_id);

// Updates or adds mime_type=app_id in the user's mimeapps.list
bool save_default_mime_association(std::string_view mime_type, std::string_view app_id);

}  // namespace broapps::linux_backend
