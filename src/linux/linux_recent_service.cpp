#include "linux_recent_service.h"
#include "xbel_parser.h"
#include <brovfs/mime.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>

namespace broapps::linux_backend {

namespace {

std::string get_default_xbel_path() {
    const char* data_home = std::getenv("XDG_DATA_HOME");
    if (data_home && *data_home != '\0') {
        return (std::filesystem::path(data_home) / "recently-used.xbel").string();
    }
    const char* home = std::getenv("HOME");
    if (home && *home != '\0') {
        return (std::filesystem::path(home) / ".local" / "share" / "recently-used.xbel").string();
    }
    return "/tmp/recently-used.xbel";
}

}  // namespace

LinuxRecentService::LinuxRecentService(std::string xbel_path)
    : xbel_path_(xbel_path.empty() ? get_default_xbel_path() : std::move(xbel_path)) {}

std::vector<RecentItem> LinuxRecentService::get_recent_items(size_t limit) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto items = parse_xbel_file(xbel_path_);

    // Reverse to show most recent first
    std::reverse(items.begin(), items.end());

    if (items.size() > limit) {
        items.resize(limit);
    }
    return items;
}

bool LinuxRecentService::add_recent_item(const std::filesystem::path& file_path, const std::string& app_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto items = parse_xbel_file(xbel_path_);

    // Remove existing entry for the same file
    std::string canonical_target;
    std::error_code ec;
    try {
        canonical_target = std::filesystem::weakly_canonical(file_path, ec).string();
    } catch (...) {
        canonical_target = file_path.string();
    }

    items.erase(std::remove_if(items.begin(), items.end(), [&](const RecentItem& it) {
        std::string it_path;
        try {
            it_path = std::filesystem::weakly_canonical(it.file_path, ec).string();
        } catch (...) {
            it_path = it.file_path.string();
        }
        return it_path == canonical_target;
    }), items.end());

    RecentItem new_item;
    new_item.file_path = file_path;
    new_item.display_name = file_path.filename().string();
    new_item.mime_type = bro::vfs::MimeDatabase::system().type_for_file(file_path).mime;
    new_item.app_id = app_id;
    new_item.timestamp = std::chrono::system_clock::now();

    items.push_back(std::move(new_item));

    if (items.size() > 500) {
        items.erase(items.begin(), items.begin() + (items.size() - 500));
    }

    return write_xbel_file(xbel_path_, items);
}

bool LinuxRecentService::clear_recent_items() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::error_code ec;
    std::filesystem::remove(xbel_path_, ec);
    return !ec;
}

}  // namespace broapps::linux_backend

#if defined(__linux__)
namespace broapps {

std::unique_ptr<RecentService> RecentService::create() {
    return std::make_unique<linux_backend::LinuxRecentService>();
}

}  // namespace broapps
#endif
