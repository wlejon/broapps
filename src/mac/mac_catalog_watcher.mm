#include "mac_catalog_watcher.h"
#include "mac_bundle.h"

#if defined(__APPLE__)
#import <CoreServices/CoreServices.h>
#import <Foundation/Foundation.h>
#import <dispatch/dispatch.h>
#include <filesystem>

namespace broapps::mac_backend {

namespace {

void fs_event_callback(
    ConstFSEventStreamRef streamRef,
    void* clientCallBackInfo,
    size_t numEvents,
    void* eventPaths,
    const FSEventStreamEventFlags eventFlags[],
    const FSEventStreamEventId eventIds[]) {

    (void)streamRef;
    (void)eventIds;
    auto* watcher = static_cast<MacCatalogWatcher*>(clientCallBackInfo);
    if (!watcher) return;

    auto paths = static_cast<char**>(eventPaths);
    std::vector<std::string> path_vec;
    std::vector<uint32_t> flag_vec;

    for (size_t i = 0; i < numEvents; ++i) {
        path_vec.push_back(paths[i]);
        flag_vec.push_back(eventFlags[i]);
    }

    watcher->on_fs_event(path_vec, flag_vec);
}

}  // namespace

MacCatalogWatcher::MacCatalogWatcher(std::shared_ptr<AppCatalog> catalog)
    : catalog_(std::move(catalog)) {}

MacCatalogWatcher::~MacCatalogWatcher() {
    stop();
}

bool MacCatalogWatcher::start() {
    if (is_watching_.load()) return true;

    auto dirs = get_mac_application_dirs();
    NSMutableArray* pathsToWatch = [NSMutableArray array];
    for (const auto& d : dirs) {
        std::error_code ec;
        if (std::filesystem::exists(d, ec)) {
            [pathsToWatch addObject:[NSString stringWithUTF8String:d.c_str()]];
        }
    }

    if ([pathsToWatch count] == 0) {
        return false;
    }

    FSEventStreamContext context = {0, this, nullptr, nullptr, nullptr};
    FSEventStreamRef stream = FSEventStreamCreate(
        nullptr,
        &fs_event_callback,
        &context,
        (__bridge CFArrayRef)pathsToWatch,
        kFSEventStreamEventIdSinceNow,
        0.5, // 500ms latency latency
        kFSEventStreamCreateFlagFileEvents | kFSEventStreamCreateFlagNoDefer);

    if (!stream) return false;

    dispatch_queue_t queue = dispatch_queue_create("com.broapps.catalogwatcher", DISPATCH_QUEUE_SERIAL);
    FSEventStreamSetDispatchQueue(stream, queue);

    if (!FSEventStreamStart(stream)) {
        FSEventStreamInvalidate(stream);
        FSEventStreamRelease(stream);
        return false;
    }

    stream_ref_ = stream;
    queue_ = (__bridge_retained void*)queue;
    is_watching_.store(true);
    return true;
}

void MacCatalogWatcher::stop() {
    if (!is_watching_.exchange(false)) return;

    if (stream_ref_) {
        auto stream = static_cast<FSEventStreamRef>(stream_ref_);
        FSEventStreamStop(stream);
        FSEventStreamInvalidate(stream);
        FSEventStreamRelease(stream);
        stream_ref_ = nullptr;
    }

    if (queue_) {
        dispatch_queue_t q = (__bridge_transfer dispatch_queue_t)queue_;
        (void)q;
        queue_ = nullptr;
    }
}

void MacCatalogWatcher::on_fs_event(const std::vector<std::string>& paths, const std::vector<uint32_t>& flags) {
    bool had_app_change = false;

    for (size_t i = 0; i < paths.size(); ++i) {
        std::string_view p = paths[i];
        if (p.find(".app") != std::string_view::npos) {
            had_app_change = true;
            auto now = std::chrono::system_clock::now();
            uint32_t flag = flags[i];

            std::string filename;
            try {
                filename = std::filesystem::path(p).filename().string();
            } catch (...) {
                filename = std::string(p);
            }

            if (flag & kFSEventStreamEventFlagItemCreated) {
                events_.push(AppAdded{filename, now});
            } else if (flag & kFSEventStreamEventFlagItemRemoved) {
                events_.push(AppRemoved{filename, now});
            } else {
                events_.push(AppModified{filename, now});
            }
        }
    }

    if (had_app_change) {
        if (catalog_) {
            catalog_->refresh();
        }
        events_.push(CatalogRefreshed{std::chrono::system_clock::now()});
    }
}

}  // namespace broapps::mac_backend

namespace broapps {

std::unique_ptr<CatalogWatcher> CatalogWatcher::create(std::shared_ptr<AppCatalog> catalog) {
    return std::make_unique<mac_backend::MacCatalogWatcher>(std::move(catalog));
}

}  // namespace broapps
#endif
