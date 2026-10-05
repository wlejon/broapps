#include "linux_catalog_watcher.h"
#include "xdg_scanner.h"

#include <chrono>
#include <cstring>
#include <filesystem>

#ifndef _WIN32
#include <fcntl.h>
#include <poll.h>
#include <sys/inotify.h>
#include <unistd.h>
#endif

namespace broapps::linux_backend {

LinuxCatalogWatcher::LinuxCatalogWatcher(std::shared_ptr<AppCatalog> catalog)
    : catalog_(std::move(catalog)) {}

LinuxCatalogWatcher::~LinuxCatalogWatcher() {
    stop();
}

bool LinuxCatalogWatcher::start() {
    if (is_watching_.load()) return true;

#ifndef _WIN32
    if (pipe2(stop_pipe_, O_CLOEXEC) < 0) {
        return false;
    }

    inotify_fd_ = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
    if (inotify_fd_ < 0) {
        close(stop_pipe_[0]);
        close(stop_pipe_[1]);
        stop_pipe_[0] = stop_pipe_[1] = -1;
        return false;
    }

    auto app_dirs = get_xdg_application_dirs();
    for (const auto& d : app_dirs) {
        std::error_code ec;
        if (std::filesystem::exists(d, ec)) {
            int wd = inotify_add_watch(inotify_fd_, d.c_str(), IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_TO | IN_MOVED_FROM);
            if (wd >= 0) {
                watch_descriptors_.push_back(wd);
            }
        }
    }

    is_watching_.store(true);
    worker_ = std::thread(&LinuxCatalogWatcher::run_loop, this);
    return true;
#else
    return false;
#endif
}

void LinuxCatalogWatcher::stop() {
    if (!is_watching_.exchange(false)) return;

#ifndef _WIN32
    if (stop_pipe_[1] >= 0) {
        char b = 1;
        ssize_t w = write(stop_pipe_[1], &b, 1);
        (void)w;
    }

    if (worker_.joinable()) {
        worker_.join();
    }

    if (inotify_fd_ >= 0) {
        for (int wd : watch_descriptors_) {
            inotify_rm_watch(inotify_fd_, wd);
        }
        watch_descriptors_.clear();
        close(inotify_fd_);
        inotify_fd_ = -1;
    }

    if (stop_pipe_[0] >= 0) { close(stop_pipe_[0]); stop_pipe_[0] = -1; }
    if (stop_pipe_[1] >= 0) { close(stop_pipe_[1]); stop_pipe_[1] = -1; }
#endif
}

void LinuxCatalogWatcher::run_loop() {
#ifndef _WIN32
    struct pollfd fds[2];
    fds[0].fd = stop_pipe_[0];
    fds[0].events = POLLIN;
    fds[1].fd = inotify_fd_;
    fds[1].events = POLLIN;

    char buffer[4096];

    while (is_watching_.load()) {
        int ret = poll(fds, 2, -1);
        if (ret <= 0) continue;

        if (fds[0].revents & POLLIN) {
            // Stop signal
            break;
        }

        if (fds[1].revents & POLLIN) {
            bool had_changes = false;
            while (true) {
                ssize_t len = read(inotify_fd_, buffer, sizeof(buffer));
                if (len <= 0) break;

                ssize_t i = 0;
                while (i < len) {
                    auto* event = reinterpret_cast<struct inotify_event*>(&buffer[i]);
                    if (event->len > 0) {
                        std::string_view name(event->name);
                        if (name.ends_with(".desktop")) {
                            had_changes = true;
                            auto now = std::chrono::system_clock::now();
                            if ((event->mask & IN_CREATE) || (event->mask & IN_MOVED_TO)) {
                                events_.push(AppAdded{std::string(name), now});
                            } else if ((event->mask & IN_DELETE) || (event->mask & IN_MOVED_FROM)) {
                                events_.push(AppRemoved{std::string(name), now});
                            } else if (event->mask & IN_MODIFY) {
                                events_.push(AppModified{std::string(name), now});
                            }
                        }
                    }
                    i += sizeof(struct inotify_event) + event->len;
                }
            }

            if (had_changes) {
                if (catalog_) {
                    catalog_->refresh();
                }
                events_.push(CatalogRefreshed{std::chrono::system_clock::now()});
            }
        }
    }
#endif
}

}  // namespace broapps::linux_backend

#if defined(__linux__)
namespace broapps {

std::unique_ptr<CatalogWatcher> CatalogWatcher::create(std::shared_ptr<AppCatalog> catalog) {
    return std::make_unique<linux_backend::LinuxCatalogWatcher>(std::move(catalog));
}

}  // namespace broapps
#endif
