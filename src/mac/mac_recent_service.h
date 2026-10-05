#pragma once

#if defined(__APPLE__)
#include "broapps/recent_service.h"
#include <mutex>
#include <string>

namespace broapps::mac_backend {

class MacRecentService : public RecentService {
public:
    explicit MacRecentService(std::string plist_path = {});
    ~MacRecentService() override = default;

    std::vector<RecentItem> get_recent_items(size_t limit = 50) override;
    bool add_recent_item(const std::filesystem::path& file_path, const std::string& app_id = {}) override;
    bool clear_recent_items() override;

private:
    std::string plist_path_;
    mutable std::recursive_mutex mutex_;
};

}  // namespace broapps::mac_backend
#endif
