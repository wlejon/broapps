#pragma once

#if defined(__APPLE__)
#include "broapps/catalog_watcher.h"

#include <atomic>
#include <memory>
#include <vector>

namespace broapps::mac_backend {

class MacCatalogWatcher : public CatalogWatcher {
public:
    explicit MacCatalogWatcher(std::shared_ptr<AppCatalog> catalog);
    ~MacCatalogWatcher() override;

    MessageQueue<CatalogEvent>& events() override { return events_; }
    bool start() override;
    void stop() override;
    bool is_watching() const override { return is_watching_.load(); }

    void on_fs_event(const std::vector<std::string>& paths, const std::vector<uint32_t>& flags);

private:
    std::shared_ptr<AppCatalog> catalog_;
    MessageQueue<CatalogEvent> events_;
    std::atomic<bool> is_watching_{false};

    void* stream_ref_ = nullptr;
    void* queue_ = nullptr;
};

}  // namespace broapps::mac_backend
#endif
