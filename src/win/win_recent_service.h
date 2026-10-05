#pragma once

#ifdef _WIN32
#include "broapps/recent_service.h"
#include <mutex>
#include <string>

namespace broapps::win_backend {

class WinRecentService : public RecentService {
public:
    WinRecentService() = default;
    ~WinRecentService() override = default;

    std::vector<RecentItem> get_recent_items(size_t limit = 50) override;
    bool add_recent_item(const std::filesystem::path& file_path, const std::string& app_id = {}) override;
    bool clear_recent_items() override;

private:
    mutable std::mutex mutex_;
};

}  // namespace broapps::win_backend
#endif
