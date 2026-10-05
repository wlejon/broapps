#pragma once

#include "broapps/app_catalog.h"
#include "broapps/event_queue.h"

#include <chrono>
#include <memory>
#include <string>
#include <variant>

namespace broapps {

struct AppAdded {
    std::string app_id;
    std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now();
};

struct AppRemoved {
    std::string app_id;
    std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now();
};

struct AppModified {
    std::string app_id;
    std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now();
};

struct CatalogRefreshed {
    std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now();
};

using CatalogEvent = std::variant<AppAdded, AppRemoved, AppModified, CatalogRefreshed>;

// Watches AppCatalog::source_directories() (recursively, through brovfs's DirectoryWatcher) and
// keeps the catalog current. app_id in AppAdded / AppRemoved / AppModified is the file name of
// the entry that changed ("Foo.lnk", "org.foo.desktop", "Foo.app"); a change inside an .app
// bundle is AppModified for the bundle. Each batch that changed the catalog (including lost
// events, which brovfs reports and which force a rescan) has already refreshed it on the
// watcher's thread when CatalogRefreshed is queued after the batch's per-entry events.
class CatalogWatcher {
public:
    virtual ~CatalogWatcher() = default;

    static std::unique_ptr<CatalogWatcher> create(std::shared_ptr<AppCatalog> catalog);

    virtual MessageQueue<CatalogEvent>& events() = 0;
    // False when none of the source directories exists or could be watched.
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool is_watching() const = 0;
};

}  // namespace broapps
