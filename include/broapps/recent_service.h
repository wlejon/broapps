#pragma once

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace broapps {

struct RecentItem {
    std::filesystem::path file_path;
    std::string uri;
    std::string display_name;
    std::string mime_type;
    std::string app_id;
    std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now();
};

class RecentService {
public:
    virtual ~RecentService() = default;

    static std::unique_ptr<RecentService> create();

    virtual std::vector<RecentItem> get_recent_items(size_t limit = 50) = 0;
    virtual bool add_recent_item(const std::filesystem::path& file_path, const std::string& app_id = {}) = 0;
    virtual bool clear_recent_items() = 0;
};

}  // namespace broapps
