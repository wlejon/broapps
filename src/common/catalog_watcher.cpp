// CatalogWatcher on brovfs's DirectoryWatcher (ReadDirectoryChangesExW / inotify / FSEvents).
//
// Every AppCatalog::source_directories() entry that exists is watched recursively. A translator
// thread turns brovfs events into catalog events: entries are recognised by name (".lnk" on
// Windows, ".desktop" on Linux, an ".app" bundle on macOS), a batch that touched an entry or
// lost events (Rescan, a new or removed directory, a root that went away) refreshes the catalog
// and ends with CatalogRefreshed.
#include "broapps/catalog_watcher.h"

#include <brovfs/path.h>
#include <brovfs/watcher.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <iterator>
#include <thread>

namespace broapps {

namespace {

namespace vfs = bro::vfs;

#if defined(_WIN32)
constexpr std::string_view kEntrySuffix = ".lnk";
#elif defined(__APPLE__)
constexpr std::string_view kEntrySuffix = ".app";
#else
constexpr std::string_view kEntrySuffix = ".desktop";
#endif

bool ends_with_ci(std::string_view s, std::string_view suffix) {
    if (s.size() < suffix.size()) return false;
    for (size_t i = 0; i < suffix.size(); ++i) {
        char a = static_cast<char>(std::tolower(static_cast<unsigned char>(s[s.size() - suffix.size() + i])));
        if (a != suffix[i]) return false;
    }
    return true;
}

// The catalog entry a path belongs to, and whether the path is the entry itself (not a file
// inside an .app bundle). Empty when the path is not part of an entry.
struct Entry {
    std::string name;
    bool is_self = false;
};

Entry entry_for(const std::filesystem::path& root, const std::filesystem::path& changed) {
    auto rel = changed.lexically_relative(root);
    if (rel.empty()) rel = changed.filename();
    Entry e;
    size_t depth = 0;
    size_t count = static_cast<size_t>(std::distance(rel.begin(), rel.end()));
    for (const auto& part : rel) {
        ++depth;
        std::string name = vfs::path_to_utf8(part);
        if (ends_with_ci(name, kEntrySuffix)) {
            e.name = std::move(name);
            e.is_self = depth == count;
            if (kEntrySuffix != ".app" && !e.is_self) e.name.clear(); // a directory named *.lnk
            return e;
        }
    }
    return e;
}

class VfsCatalogWatcher : public CatalogWatcher {
public:
    explicit VfsCatalogWatcher(std::shared_ptr<AppCatalog> catalog)
        : catalog_(std::move(catalog)), queue_(std::make_shared<vfs::WatchEventQueue>()) {}

    ~VfsCatalogWatcher() override { stop(); }

    MessageQueue<CatalogEvent>& events() override { return events_; }
    bool is_watching() const override { return watching_; }

    bool start() override {
        if (watching_) return true;
        if (!catalog_) return false;
        auto watcher = std::make_unique<vfs::DirectoryWatcher>(queue_);
        roots_.clear();
        for (const auto& dir : catalog_->source_directories()) {
            std::error_code ec;
            if (!std::filesystem::is_directory(dir, ec)) continue;
            vfs::WatchOptions opts;
            opts.recursive = true;
            opts.latency = std::chrono::milliseconds(100);
            vfs::WatchId id = watcher->add(dir, opts, ec);
            if (id != 0) roots_.emplace_back(id, dir);
        }
        if (roots_.empty()) return false;
        queue_->drain();
        watcher_ = std::move(watcher);
        watching_ = true;
        thread_ = std::thread([this] { run(); });
        return true;
    }

    void stop() override {
        if (!watching_) return;
        watching_ = false;
        // WatchId 0 is never a real watch: it tells the translator thread to exit.
        vfs::WatchEvent wake;
        wake.watch = 0;
        queue_->push(std::move(wake));
        if (thread_.joinable()) thread_.join();
        watcher_.reset(); // removes every watch; nothing is pushed after this returns
        queue_->drain();
    }

private:
    const std::filesystem::path* root_of(vfs::WatchId id) const {
        for (const auto& [rid, dir] : roots_) {
            if (rid == id) return &dir;
        }
        return nullptr;
    }

    void run() {
        for (;;) {
            queue_->wait_for(std::chrono::hours(1));
            bool quit = false;
            bool refresh = false;
            std::vector<CatalogEvent> out;
            auto now = std::chrono::system_clock::now();
            for (auto& ev : queue_->drain()) {
                if (ev.watch == 0) {
                    quit = true;
                    continue;
                }
                const auto* root = root_of(ev.watch);
                if (!root) continue;
                using K = vfs::WatchEventKind;
                switch (ev.kind) {
                    case K::Rescan:
                    case K::RootRemoved:
                    case K::Error:
                        refresh = true;
                        break;
                    case K::Renamed: {
                        Entry from = entry_for(*root, ev.old_path);
                        Entry to = entry_for(*root, ev.path);
                        if (!from.name.empty() && from.name != to.name) out.push_back(AppRemoved{from.name, now});
                        if (!to.name.empty() && from.name != to.name) out.push_back(AppAdded{to.name, now});
                        if (!to.name.empty() && from.name == to.name) out.push_back(AppModified{to.name, now});
                        refresh = refresh || !from.name.empty() || !to.name.empty() ||
                                  ev.file_kind == vfs::FileKind::Directory;
                        break;
                    }
                    case K::Created:
                    case K::Removed:
                    case K::Modified: {
                        Entry e = entry_for(*root, ev.path);
                        if (!e.name.empty()) {
                            refresh = true;
                            if (!e.is_self || ev.kind == K::Modified) {
                                out.push_back(AppModified{e.name, now});
                            } else if (ev.kind == K::Created) {
                                out.push_back(AppAdded{e.name, now});
                            } else {
                                out.push_back(AppRemoved{e.name, now});
                            }
                        } else if (ev.kind != K::Modified && ev.file_kind == vfs::FileKind::Directory) {
                            refresh = true; // a subtree came or went: entries may be inside it
                        }
                        break;
                    }
                }
            }
            if (refresh) {
                catalog_->refresh();
                out.push_back(CatalogRefreshed{std::chrono::system_clock::now()});
            }
            for (auto& e : out) events_.push(std::move(e));
            if (quit) return;
        }
    }

    std::shared_ptr<AppCatalog> catalog_;
    std::shared_ptr<vfs::WatchEventQueue> queue_;
    std::unique_ptr<vfs::DirectoryWatcher> watcher_;
    std::vector<std::pair<vfs::WatchId, std::filesystem::path>> roots_;
    MessageQueue<CatalogEvent> events_;
    std::atomic<bool> watching_{false};
    std::thread thread_;
};

}  // namespace

std::unique_ptr<CatalogWatcher> CatalogWatcher::create(std::shared_ptr<AppCatalog> catalog) {
    return std::make_unique<VfsCatalogWatcher>(std::move(catalog));
}

}  // namespace broapps
