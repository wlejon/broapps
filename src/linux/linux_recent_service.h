#pragma once

#include "broapps/recent_service.h"
#include <mutex>
#include <string>

namespace broapps::linux_backend {

class LinuxRecentService : public RecentService {
public:
    explicit LinuxRecentService(std::string xbel_path = {});
    ~LinuxRecentService() override = default;

    std::vector<RecentItem> get_recent_items(size_t limit = 50) override;
    bool add_recent_item(const std::filesystem::path& file_path, const std::string& app_id = {}) override;
    bool clear_recent_items() override;

private:
    std::string xbel_path_;
    mutable std::mutex mutex_;
};

}  // namespace broapps::linux_backend
