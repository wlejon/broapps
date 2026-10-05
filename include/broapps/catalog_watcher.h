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

class CatalogWatcher {
public:
    virtual ~CatalogWatcher() = default;

    static std::unique_ptr<CatalogWatcher> create(std::shared_ptr<AppCatalog> catalog);

    virtual MessageQueue<CatalogEvent>& events() = 0;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool is_watching() const = 0;
};

}  // namespace broapps
