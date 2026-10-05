#pragma once

#ifdef _WIN32
#include "broapps/catalog_watcher.h"

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

namespace broapps::win_backend {

class WinCatalogWatcher : public CatalogWatcher {
public:
    explicit WinCatalogWatcher(std::shared_ptr<AppCatalog> catalog);
    ~WinCatalogWatcher() override;

    MessageQueue<CatalogEvent>& events() override { return events_; }
    bool start() override;
    void stop() override;
    bool is_watching() const override { return is_watching_.load(); }

private:
    std::shared_ptr<AppCatalog> catalog_;
    MessageQueue<CatalogEvent> events_;
    std::atomic<bool> is_watching_{false};

    void* stop_event_ = nullptr;
    std::thread worker_;

    void run_loop();
};

}  // namespace broapps::win_backend
#endif
