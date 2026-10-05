// CatalogWatcher on a real directory: a catalog with a private extra search path is watched, an
// application entry (.lnk / .desktop / .app bundle, the real format each platform's catalog
// parses) is created, edited and removed there, and each change must arrive as AppAdded /
// AppModified / AppRemoved followed by CatalogRefreshed, with the catalog already updated.
// Nothing outside the test's own temp directory is touched.
#include "broapps/app_catalog.h"
#include "broapps/catalog_watcher.h"
#include "tests/test_common.h"

#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

#ifdef _WIN32
#include "src/win/com_init.h"
#include <shlobj.h>
#endif

namespace fs = std::filesystem;
using namespace broapps;

namespace {

const char* kName = "BroappsWatchProbe";

#if defined(_WIN32)
const char* kEntry = "BroappsWatchProbe.lnk";
bool make_entry(const fs::path& dir, const char* comment) {
    win_backend::ComScope com;
    IShellLinkW* link = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link)))) return false;
    wchar_t sys[MAX_PATH];
    GetSystemDirectoryW(sys, MAX_PATH);
    std::wstring target = std::wstring(sys) + L"\\notepad.exe";
    link->SetPath(target.c_str());
    std::wstring wcomment(comment, comment + std::strlen(comment));
    link->SetDescription(wcomment.c_str());
    IPersistFile* file = nullptr;
    bool ok = SUCCEEDED(link->QueryInterface(IID_PPV_ARGS(&file)));
    if (ok) {
        ok = SUCCEEDED(file->Save((dir / kEntry).wstring().c_str(), TRUE));
        file->Release();
    }
    link->Release();
    return ok;
}
#elif defined(__APPLE__)
const char* kEntry = "BroappsWatchProbe.app";
bool make_entry(const fs::path& dir, const char* comment) {
    fs::path contents = dir / kEntry / "Contents";
    std::error_code ec;
    fs::create_directories(contents / "MacOS", ec);
    std::ofstream plist(contents / "Info.plist");
    plist << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
             "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
             "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
             "<plist version=\"1.0\"><dict>\n"
             "<key>CFBundleIdentifier</key><string>dev.bro.broapps-watch-probe</string>\n"
             "<key>CFBundleName</key><string>"
          << kName << "</string>\n<key>CFBundleExecutable</key><string>probe</string>\n"
          << "<key>CFBundleGetInfoString</key><string>" << comment << "</string>\n</dict></plist>\n";
    return plist.good();
}
#else
const char* kEntry = "broapps-watch-probe.desktop";
bool make_entry(const fs::path& dir, const char* comment) {
    std::ofstream f(dir / kEntry);
    f << "[Desktop Entry]\nType=Application\nName=" << kName << "\nComment=" << comment << "\nExec=/bin/true\n";
    return f.good();
}
#endif

bool in_catalog(const AppCatalog& catalog) {
    for (const auto& a : catalog.apps()) {
        if (a.name == kName) return true;
    }
    return false;
}

// Changed: an in-place edit, which a backend may report as a modification or (when the writer
// replaces the file) as the entry appearing again.
enum class Want { Added, Changed, Removed };

// Waits for the entry's event of the wanted kind and a CatalogRefreshed after it.
bool wait_for(CatalogWatcher& w, Want want, std::chrono::seconds timeout) {
    bool seen = false;
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        w.events().wait_for(std::chrono::milliseconds(200));
        for (auto& ev : w.events().drain()) {
            if (auto* a = std::get_if<AppAdded>(&ev); a && a->app_id == kEntry && want != Want::Removed) seen = true;
            if (auto* m = std::get_if<AppModified>(&ev); m && m->app_id == kEntry && want == Want::Changed) seen = true;
            if (auto* r = std::get_if<AppRemoved>(&ev); r && r->app_id == kEntry && want == Want::Removed) seen = true;
            if (seen && std::holds_alternative<CatalogRefreshed>(ev)) return true;
        }
    }
    return false;
}

}  // namespace

int main() {
    fs::path dir = fs::temp_directory_path() /
                   ("broapps_watch_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(dir);

    {
        std::shared_ptr<AppCatalog> catalog = AppCatalog::create({{dir.string()}, true});
        TEST_CHECK(catalog != nullptr);
        TEST_CHECK(!in_catalog(*catalog));
        auto dirs = catalog->source_directories();
        TEST_CHECK(!dirs.empty() && dirs.front() == dir);

        auto watcher = CatalogWatcher::create(catalog);
        TEST_CHECK(!watcher->is_watching());
        TEST_CHECK(watcher->start());
        TEST_CHECK(watcher->is_watching());
        TEST_CHECK(watcher->start()); // idempotent

        TEST_CHECK(make_entry(dir, "first"));
        TEST_CHECK(wait_for(*watcher, Want::Added, std::chrono::seconds(10)));
        TEST_CHECK(in_catalog(*catalog));

        TEST_CHECK(make_entry(dir, "second"));
        TEST_CHECK(wait_for(*watcher, Want::Changed, std::chrono::seconds(10)));
        TEST_CHECK(in_catalog(*catalog));

        std::error_code ec;
        fs::remove_all(dir / kEntry, ec);
        TEST_CHECK(!ec);
        TEST_CHECK(wait_for(*watcher, Want::Removed, std::chrono::seconds(10)));
        TEST_CHECK(!in_catalog(*catalog));

        // Non-entries are ignored.
        { std::ofstream(dir / "notes.txt") << "x"; }

        watcher->stop();
        TEST_CHECK(!watcher->is_watching());
        watcher->events().drain();
        TEST_CHECK(make_entry(dir, "after stop"));
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        TEST_CHECK(watcher->events().empty());

        // Restartable after stop.
        TEST_CHECK(watcher->start());
        watcher->stop();
    }

    std::error_code ec;
    fs::remove_all(dir, ec);
    std::cout << "test_catalog_watcher passed!" << std::endl;
    return 0;
}
