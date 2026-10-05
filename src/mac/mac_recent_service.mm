#include "mac_recent_service.h"
#include <brovfs/mime.h>

#if defined(__APPLE__)
#import <Foundation/Foundation.h>
#include <algorithm>
#include <cstdlib>
#include <filesystem>

namespace broapps::mac_backend {

namespace {

std::string get_default_mac_recent_path() {
    const char* home = std::getenv("HOME");
    if (home) {
        return (std::filesystem::path(home) / "Library" / "Application Support" / "broapps" / "recent_items.plist").string();
    }
    return "/tmp/broapps_recent_items.plist";
}

}  // namespace

MacRecentService::MacRecentService(std::string plist_path)
    : plist_path_(plist_path.empty() ? get_default_mac_recent_path() : std::move(plist_path)) {}

std::vector<RecentItem> MacRecentService::get_recent_items(size_t limit) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<RecentItem> items;

    @autoreleasepool {
        NSString* path = [NSString stringWithUTF8String:plist_path_.c_str()];
        NSArray* arr = [NSArray arrayWithContentsOfFile:path];
        if (!arr) return items;

        for (NSDictionary* dict in arr) {
            if (items.size() >= limit) break;

            RecentItem item;
            NSString* p = dict[@"path"];
            if (p) item.file_path = std::filesystem::path([p UTF8String]);
            NSString* u = dict[@"uri"];
            if (u) item.uri = [u UTF8String];
            NSString* d = dict[@"displayName"];
            if (d) item.display_name = [d UTF8String];
            NSString* m = dict[@"mimeType"];
            if (m) item.mime_type = [m UTF8String];
            NSString* a = dict[@"appId"];
            if (a) item.app_id = [a UTF8String];

            NSNumber* t = dict[@"timestamp"];
            if (t) {
                auto ms = std::chrono::milliseconds([t longLongValue]);
                item.timestamp = std::chrono::system_clock::time_point(ms);
            }

            items.push_back(std::move(item));
        }
    }

    return items;
}

bool MacRecentService::add_recent_item(const std::filesystem::path& file_path, const std::string& app_id) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    auto items = get_recent_items(500);

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
    new_item.uri = "file://" + file_path.generic_string();
    new_item.mime_type = bro::vfs::MimeDatabase::system().type_for_file(file_path).mime;
    new_item.app_id = app_id;
    new_item.timestamp = std::chrono::system_clock::now();

    items.insert(items.begin(), std::move(new_item));
    if (items.size() > 500) items.resize(500);

    @autoreleasepool {
        NSMutableArray* arr = [NSMutableArray arrayWithCapacity:items.size()];
        for (const auto& it : items) {
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(it.timestamp.time_since_epoch()).count();
            NSDictionary* d = @{
                @"path": [NSString stringWithUTF8String:it.file_path.string().c_str()],
                @"uri": [NSString stringWithUTF8String:it.uri.c_str()],
                @"displayName": [NSString stringWithUTF8String:it.display_name.c_str()],
                @"mimeType": [NSString stringWithUTF8String:it.mime_type.c_str()],
                @"appId": [NSString stringWithUTF8String:it.app_id.c_str()],
                @"timestamp": @(ms)
            };
            [arr addObject:d];
        }

        std::filesystem::path parent_p = std::filesystem::path(plist_path_).parent_path();
        std::filesystem::create_directories(parent_p, ec);

        NSString* path = [NSString stringWithUTF8String:plist_path_.c_str()];
        return [arr writeToFile:path atomically:YES];
    }
}

bool MacRecentService::clear_recent_items() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::error_code ec;
    std::filesystem::remove(plist_path_, ec);
    return !ec;
}

}  // namespace broapps::mac_backend

namespace broapps {

std::unique_ptr<RecentService> RecentService::create() {
    return std::make_unique<mac_backend::MacRecentService>();
}

}  // namespace broapps
#endif
