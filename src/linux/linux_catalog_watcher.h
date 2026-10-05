#pragma once

#include "broapps/catalog_watcher.h"

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace broapps::linux_backend {

class LinuxCatalogWatcher : public CatalogWatcher {
public:
    explicit LinuxCatalogWatcher(std::shared_ptr<AppCatalog> catalog);
    ~LinuxCatalogWatcher() override;

    MessageQueue<CatalogEvent>& events() override { return events_; }
    bool start() override;
    void stop() override;
    bool is_watching() const override { return is_watching_.load(); }

private:
    std::shared_ptr<AppCatalog> catalog_;
    MessageQueue<CatalogEvent> events_;
    std::atomic<bool> is_watching_{false};

    int inotify_fd_ = -1;
    int stop_pipe_[2] = {-1, -1};
    std::thread worker_;
    std::vector<int> watch_descriptors_;

    void run_loop();
};

}  // namespace broapps::linux_backend
