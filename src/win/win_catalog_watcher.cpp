#include "win_catalog_watcher.h"
#include "lnk_parser.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <filesystem>

namespace broapps::win_backend {

namespace {

std::string wide_to_utf8(std::wstring_view wstr) {
    if (wstr.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string out(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), out.data(), size, nullptr, nullptr);
    return out;
}

}  // namespace

WinCatalogWatcher::WinCatalogWatcher(std::shared_ptr<AppCatalog> catalog)
    : catalog_(std::move(catalog)) {}

WinCatalogWatcher::~WinCatalogWatcher() {
    stop();
}

bool WinCatalogWatcher::start() {
    if (is_watching_.load()) return true;

    stop_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!stop_event_) return false;

    is_watching_.store(true);
    worker_ = std::thread(&WinCatalogWatcher::run_loop, this);
    return true;
}

void WinCatalogWatcher::stop() {
    if (!is_watching_.exchange(false)) return;

    if (stop_event_) {
        SetEvent(static_cast<HANDLE>(stop_event_));
    }

    if (worker_.joinable()) {
        worker_.join();
    }

    if (stop_event_) {
        CloseHandle(static_cast<HANDLE>(stop_event_));
        stop_event_ = nullptr;
    }
}

void WinCatalogWatcher::run_loop() {
    auto dirs = get_start_menu_dirs();

    struct WatchedDir {
        HANDLE hDir = INVALID_HANDLE_VALUE;
        OVERLAPPED ov = {};
        HANDLE hEvent = nullptr;
        BYTE buffer[4096] = {};
        bool pending = false;
    };

    std::vector<WatchedDir> watches;
    std::vector<HANDLE> wait_handles;
    wait_handles.push_back(static_cast<HANDLE>(stop_event_));

    for (const auto& d : dirs) {
        HANDLE hDir = CreateFileW(
            d.c_str(),
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
            nullptr);

        if (hDir != INVALID_HANDLE_VALUE) {
            WatchedDir wd;
            wd.hDir = hDir;
            wd.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            wd.ov.hEvent = wd.hEvent;
            watches.push_back(wd);
            wait_handles.push_back(wd.hEvent);
        }
    }

    if (watches.empty()) {
        return;
    }

    for (auto& w : watches) {
        DWORD bytes = 0;
        BOOL ok = ReadDirectoryChangesW(
            w.hDir,
            w.buffer,
            sizeof(w.buffer),
            TRUE,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
            &bytes,
            &w.ov,
            nullptr);
        w.pending = ok || (GetLastError() == ERROR_IO_PENDING);
    }

    while (is_watching_.load()) {
        DWORD res = WaitForMultipleObjects(
            static_cast<DWORD>(wait_handles.size()),
            wait_handles.data(),
            FALSE,
            INFINITE);

        if (res == WAIT_OBJECT_0) {
            // Stop event signalled
            break;
        }

        if (res > WAIT_OBJECT_0 && res < WAIT_OBJECT_0 + wait_handles.size()) {
            size_t watch_idx = (res - WAIT_OBJECT_0) - 1;
            auto& w = watches[watch_idx];
            ResetEvent(w.hEvent);

            DWORD bytes_transferred = 0;
            if (GetOverlappedResult(w.hDir, &w.ov, &bytes_transferred, FALSE) && bytes_transferred > 0) {
                BYTE* p = w.buffer;
                bool had_changes = false;

                while (p) {
                    auto* fni = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(p);
                    std::wstring_view fname(fni->FileName, fni->FileNameLength / sizeof(wchar_t));

                    if (fname.ends_with(L".lnk")) {
                        had_changes = true;
                        std::string name_utf8 = wide_to_utf8(fname);
                        auto now = std::chrono::system_clock::now();

                        if (fni->Action == FILE_ACTION_ADDED || fni->Action == FILE_ACTION_RENAMED_NEW_NAME) {
                            events_.push(AppAdded{name_utf8, now});
                        } else if (fni->Action == FILE_ACTION_REMOVED || fni->Action == FILE_ACTION_RENAMED_OLD_NAME) {
                            events_.push(AppRemoved{name_utf8, now});
                        } else if (fni->Action == FILE_ACTION_MODIFIED) {
                            events_.push(AppModified{name_utf8, now});
                        }
                    }

                    if (fni->NextEntryOffset == 0) break;
                    p += fni->NextEntryOffset;
                }

                if (had_changes) {
                    if (catalog_) {
                        catalog_->refresh();
                    }
                    events_.push(CatalogRefreshed{std::chrono::system_clock::now()});
                }
            }

            // Re-arm ReadDirectoryChangesW
            if (is_watching_.load()) {
                DWORD bytes = 0;
                ReadDirectoryChangesW(
                    w.hDir,
                    w.buffer,
                    sizeof(w.buffer),
                    TRUE,
                    FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
                    &bytes,
                    &w.ov,
                    nullptr);
            }
        }
    }

    for (auto& w : watches) {
        CancelIo(w.hDir);
        CloseHandle(w.hDir);
        CloseHandle(w.hEvent);
    }
}

}  // namespace broapps::win_backend

namespace broapps {

std::unique_ptr<CatalogWatcher> CatalogWatcher::create(std::shared_ptr<AppCatalog> catalog) {
    return std::make_unique<win_backend::WinCatalogWatcher>(std::move(catalog));
}

}  // namespace broapps
#endif
