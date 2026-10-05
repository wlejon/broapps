#pragma once

#include "broapps/recent_service.h"
#include <string>
#include <string_view>
#include <vector>

namespace broapps::linux_backend {

// Parses recently-used.xbel XML content into RecentItem entries
std::vector<RecentItem> parse_xbel_string(std::string_view xml_content);

// Parses an XBEL file from disk
std::vector<RecentItem> parse_xbel_file(const std::string& file_path);

// Serializes RecentItem entries into XBEL XML string format
std::string serialize_xbel_string(const std::vector<RecentItem>& items);

// Writes RecentItem entries to an XBEL file on disk atomically
bool write_xbel_file(const std::string& file_path, const std::vector<RecentItem>& items);

}  // namespace broapps::linux_backend
