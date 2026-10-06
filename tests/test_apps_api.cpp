#include "../src/api/api.h"
#include "embed/embed.h"
#include "eval/eval.h"
#include "broapps/broapps.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::cerr << "CHECK failed: " #cond " (line " << __LINE__ << ")" \
                      << std::endl;                                        \
            std::exit(1);                                                  \
        }                                                                  \
    } while (0)

int main() {
    namespace ev = bronze::embed;
    using namespace bronze::eval;

    std::cout << "Starting broapps JavaScript API test..." << std::endl;

#if !defined(__linux__)
    // Off Linux the recent-items and default-app services act on the real
    // account (SHAddToRecentDocs, file associations, LaunchServices) with no
    // per-process redirect, and this test calls clearRecent / addRecent /
    // setDefaultApp. It runs there only when explicitly asked to.
    {
        const char* mutate = std::getenv("BROAPPS_TEST_MUTATE");
        if (!mutate || std::string(mutate) != "1") {
            std::cout << "SKIPPED: would change this account's recent files and file "
                         "associations; set BROAPPS_TEST_MUTATE=1 to run it anyway"
                      << std::endl;
            return 77;
        }
    }
#endif

    // 1. Create a temporary directory with a test desktop file
    auto tmp_dir = std::filesystem::temp_directory_path() /
                   ("broapps_api_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(tmp_dir);

    // Everything this test writes (mimeapps.list via setDefaultApp*,
    // recently-used.xbel via addRecent/clearRecent) must land in tmp_dir, never
    // in the account running it: both XDG homes point there before any
    // service is created.
    const char* orig_config_home = std::getenv("XDG_CONFIG_HOME");
    std::string orig_config_str = orig_config_home ? orig_config_home : "";
    const char* orig_data_home = std::getenv("XDG_DATA_HOME");
    std::string orig_data_str = orig_data_home ? orig_data_home : "";
    setenv("XDG_CONFIG_HOME", (tmp_dir / "config").c_str(), 1);
    setenv("XDG_DATA_HOME", (tmp_dir / "data").c_str(), 1);
    std::filesystem::create_directories(tmp_dir / "config");
    std::filesystem::create_directories(tmp_dir / "data");

    auto test_desktop_file = tmp_dir / "test-bro-calc.desktop";
    {
        std::ofstream ofs(test_desktop_file);
        ofs << "[Desktop Entry]\n"
            << "Version=1.0\n"
            << "Type=Application\n"
            << "Name=Bro Calculator\n"
            << "GenericName=Calculator\n"
            << "Comment=Perform arithmetic\n"
            << "Exec=/bin/echo \"calculating\"\n"
            << "Icon=accessories-calculator\n"
            << "Terminal=false\n"
            << "Categories=Utility;Calculator;\n"
            << "Keywords=math;calc;\n"
            << "MimeType=application/x-calc;text/plain;\n"
            << "NoDisplay=false\n"
            << "\n"
            << "[Desktop Action Quick]\n"
            << "Name=Quick Math\n"
            << "Exec=/bin/echo \"quick\"\n"
            << "Icon=quick-icon\n";
    }

    // Configure catalog with temporary directory
    broapps::CatalogConfig cat_cfg;
    cat_cfg.extra_search_paths = {tmp_dir.string()};
    cat_cfg.include_nodisplay = true;

    auto test_catalog = broapps::AppCatalog::create(cat_cfg);
    CHECK(test_catalog != nullptr);
    broapps::api::setCatalog(std::move(test_catalog));

    // 2. Install bro.apps into Bronze realm
    broapps::api::installApps();

    auto g = ev::globalValue("bro");
    CHECK(g.found);
    CHECK(ev::isObject(g.value));

    ev::Persistent apps(ev::getProperty(g.value, "apps"));
    CHECK(ev::isObject(apps.get()));
    std::cout << "  Mounted bro.apps successfully." << std::endl;

    // Verify all core methods exist
    const char* methods[] = {
        "list", "get", "search", "launch", "resolveIcon",
        "getDefaultApp", "setDefaultAppForMime", "setDefaultApp",
        "getAppsForMime", "getRecent", "addRecent",
        "clearRecent", "watch", "unwatch", "refresh",
        "findByCategory", "findByMimeType", "getCapabilities"
    };
    for (const char* m : methods) {
        auto fn = ev::getProperty(apps.get(), m);
        CHECK(ev::isFunction(fn));
        std::cout << "  Found bro.apps." << m << std::endl;
    }

    // Verify ProcessHandle constructor exists
    auto phCtor = ev::getProperty(apps.get(), "ProcessHandle");
    CHECK(ev::isFunction(phCtor));
    std::cout << "  Found bro.apps.ProcessHandle constructor." << std::endl;

    // 3. Test list(), get(), search(), findByCategory(), findByMimeType()
    std::cout << "Testing list, get, search, categories..." << std::endl;
    {
        auto r = evalScript(
            "(function() {\n"
            "  const list = bro.apps.list();\n"
            "  if (!Array.isArray(list)) return false;\n"
            "  const app = bro.apps.get('test-bro-calc.desktop');\n"
            "  if (!app) return false;\n"
            "  if (app.id !== 'test-bro-calc.desktop') return false;\n"
            "  if (app.name !== 'Bro Calculator') return false;\n"
            "  if (app.genericName !== 'Calculator') return false;\n"
            "  if (app.comment !== 'Perform arithmetic') return false;\n"
            "  if (app.icon !== 'accessories-calculator') return false;\n"
            "  if (app.terminal !== false) return false;\n"
            "  if (app.nodisplay !== false) return false;\n"
            "  if (!Array.isArray(app.categories) || !app.categories.includes('Calculator')) return false;\n"
            "  if (!Array.isArray(app.keywords) || !app.keywords.includes('calc')) return false;\n"
            "  if (!Array.isArray(app.mimeTypes) || !app.mimeTypes.includes('application/x-calc')) return false;\n"
            "  if (!Array.isArray(app.actions) || app.actions.length !== 1) return false;\n"
            "  if (app.actions[0].name !== 'Quick Math') return false;\n"
            "  \n"
            "  const sResults = bro.apps.search('Calculator');\n"
            "  if (!Array.isArray(sResults) || sResults.length === 0) return false;\n"
            "  if (!sResults.some(a => a.id === 'test-bro-calc.desktop')) return false;\n"
            "  \n"
            "  const catResults = bro.apps.findByCategory('Calculator');\n"
            "  if (!Array.isArray(catResults) || !catResults.some(a => a.id === 'test-bro-calc.desktop')) return false;\n"
            "  \n"
            "  const mimeResults = bro.apps.findByMimeType('application/x-calc');\n"
            "  if (!Array.isArray(mimeResults) || !mimeResults.some(a => a.id === 'test-bro-calc.desktop')) return false;\n"
            "  \n"
            "  if (bro.apps.get('nonexistent-app.desktop') !== null) return false;\n"
            "  return true;\n"
            "})()\n"
        );
        CHECK(!r.thrown);
        CHECK(ev::isBool(r.value) && ev::toBool(r.value));
        std::cout << "  list, get, search [PASS]" << std::endl;
    }

    // 4. Test capabilities
    std::cout << "Testing getCapabilities..." << std::endl;
    {
        auto r = evalScript(
            "(function() {\n"
            "  const caps = bro.apps.getCapabilities();\n"
            "  if (!caps || typeof caps !== 'object') return false;\n"
            "  if (!caps.launcher || typeof caps.launcher !== 'object') return false;\n"
            "  if (!caps.catalog || typeof caps.catalog !== 'object') return false;\n"
            "  if (typeof caps.catalog.supportsCategories !== 'boolean') return false;\n"
            "  if (typeof caps.launcher.hasTerminalLaunch !== 'boolean') return false;\n"
            "  return true;\n"
            "})()\n"
        );
        CHECK(!r.thrown);
        CHECK(ev::isBool(r.value) && ev::toBool(r.value));
        std::cout << "  getCapabilities [PASS]" << std::endl;
    }

    // 5. Test MIME associations
    std::cout << "Testing getDefaultApp, setDefaultAppForMime, and getAppsForMime..." << std::endl;
    {
        auto r = evalScript(
            "(function() {\n"
            "  const cands = bro.apps.getAppsForMime('application/x-calc');\n"
            "  if (!Array.isArray(cands)) return false;\n"
            "  if (!cands.some(a => a.id === 'test-bro-calc.desktop')) return false;\n"
            "  \n"
            "  const setOk = bro.apps.setDefaultAppForMime('application/x-calc', 'test-bro-calc.desktop');\n"
            "  if (setOk !== true) return false;\n"
            "  \n"
            "  const defApp = bro.apps.getDefaultApp('application/x-calc');\n"
            "  if (!defApp || typeof defApp !== 'object') return false;\n"
            "  if (defApp.id !== 'test-bro-calc.desktop') return false;\n"
            "  \n"
            "  const setAliasOk = bro.apps.setDefaultApp('application/x-calc', 'test-bro-calc.desktop');\n"
            "  if (setAliasOk !== true) return false;\n"
            "  return true;\n"
            "})()\n"
        );
        CHECK(!r.thrown);
        CHECK(ev::isBool(r.value) && ev::toBool(r.value));
        std::cout << "  MIME associations [PASS]" << std::endl;
    }

    // 6. Test recent items
    std::cout << "Testing recent items..." << std::endl;
    {
        auto r = evalScript(
            "(function() {\n"
            "  bro.apps.clearRecent();\n"
            "  bro.apps.addRecent('/tmp/test_recent_doc.txt', 'test-bro-calc.desktop');\n"
            "  const recents = bro.apps.getRecent(10);\n"
            "  if (!Array.isArray(recents)) return false;\n"
            "  if (recents.length === 0) return false;\n"
            "  const item = recents[0];\n"
            "  if (item.filePath !== '/tmp/test_recent_doc.txt') return false;\n"
            "  if (item.appId !== 'test-bro-calc.desktop') return false;\n"
            "  if (typeof item.timestamp !== 'number') return false;\n"
            "  return true;\n"
            "})()\n"
        );
        CHECK(!r.thrown);
        CHECK(ev::isBool(r.value) && ev::toBool(r.value));
        std::cout << "  recent items [PASS]" << std::endl;
    }

    // 7. Test resolveIcon
    std::cout << "Testing resolveIcon..." << std::endl;
    {
        auto r = evalScript(
            "(function() {\n"
            "  const res = bro.apps.resolveIcon('accessories-calculator', { size: 48 });\n"
            "  if (res !== null && typeof res !== 'string') return false;\n"
            "  const resNonExistent = bro.apps.resolveIcon('some_nonexistent_icon_12345');\n"
            "  if (resNonExistent !== null) return false;\n"
            "  return true;\n"
            "})()\n"
        );
        CHECK(!r.thrown);
        CHECK(ev::isBool(r.value) && ev::toBool(r.value));
        std::cout << "  resolveIcon [PASS]" << std::endl;
    }

    // 8. Test launch Promise (success and rejection) and ProcessHandle HostClass
    std::cout << "Testing launch Promise and ProcessHandle..." << std::endl;
    {
        // 8a. Rejection when app not found
        auto rReject = evalScript(
            "(function() {\n"
            "  globalThis._launchRejected = false;\n"
            "  bro.apps.launch('definitely-not-an-app-id-xyz').catch((err) => {\n"
            "    globalThis._launchRejected = true;\n"
            "  });\n"
            "  return true;\n"
            "})()\n"
        );
        CHECK(!rReject.thrown);
        ev::drainMicrotasks();

        auto rCheckReject = evalScript("globalThis._launchRejected === true;");
        CHECK(!rCheckReject.thrown && ev::toBool(rCheckReject.value));
        std::cout << "  launch rejection [PASS]" << std::endl;

        // 8b. Launch success
        auto rSuccess = evalScript(
            "(function() {\n"
            "  globalThis._launchSuccess = false;\n"
            "  globalThis._procHandle = null;\n"
            "  bro.apps.launch('test-bro-calc.desktop', {\n"
            "    args: ['arg1', 'arg2'],\n"
            "    env: { TEST_VAR: '123' },\n"
            "    cwd: '/tmp'\n"
            "  }).then((handle) => {\n"
            "    globalThis._launchSuccess = true;\n"
            "    globalThis._procHandle = handle;\n"
            "  }).catch((err) => {\n"
            "    globalThis._launchError = String(err);\n"
            "  });\n"
            "  return true;\n"
            "})()\n"
        );
        CHECK(!rSuccess.thrown);
        ev::drainMicrotasks();

        auto rCheckSuccess = evalScript(
            "(function() {\n"
            "  if (!globalThis._launchSuccess) return false;\n"
            "  const h = globalThis._procHandle;\n"
            "  if (!h || typeof h !== 'object') return false;\n"
            "  if (!(h instanceof bro.apps.ProcessHandle)) return false;\n"
            "  if (typeof h.pid !== 'function' || h.pid() <= 0) return false;\n"
            "  if (typeof h.launchId !== 'function' || h.launchId() < 0) return false;\n"
            "  if (typeof h.scopeId !== 'function') return false;\n"
            "  if (typeof h.isRunning !== 'function') return false;\n"
            "  if (typeof h.terminate !== 'function') return false;\n"
            "  if (typeof h.kill !== 'function') return false;\n"
            "  if (typeof h.wait !== 'function') return false;\n"
            "  if (typeof h.exitCode !== 'function') return false;\n"
            "  \n"
            "  // Wait for echo to finish\n"
            "  h.wait(5000);\n"
            "  if (h.isRunning() !== false) return false;\n"
            "  if (h.exitCode() !== 0) return false;\n"
            "  return true;\n"
            "})()\n"
        );
        CHECK(!rCheckSuccess.thrown);
        CHECK(ev::isBool(rCheckSuccess.value) && ev::toBool(rCheckSuccess.value));
        std::cout << "  launch and ProcessHandle methods [PASS]" << std::endl;
    }

    // 9. Test watch, unwatch and tickAppsAsync
    std::cout << "Testing watch, unwatch, and tickAppsAsync..." << std::endl;
    {
        auto rWatchInit = evalScript(
            "(function() {\n"
            "  globalThis._watchEvents = [];\n"
            "  globalThis._watchHandle = bro.apps.watch((ev) => {\n"
            "    globalThis._watchEvents.push(ev);\n"
            "  });\n"
            "  return (typeof globalThis._watchHandle === 'object' && typeof globalThis._watchHandle.token === 'number');\n"
            "})()\n"
        );
        CHECK(!rWatchInit.thrown);
        CHECK(ev::isBool(rWatchInit.value) && ev::toBool(rWatchInit.value));

        auto watcher = broapps::api::getCatalogWatcher();
        CHECK(watcher != nullptr);

        // Inject simulated catalog events directly into queue
        watcher->events().push(broapps::AppAdded{"test-app-2.desktop", std::chrono::system_clock::now()});
        watcher->events().push(broapps::AppModified{"test-bro-calc.desktop", std::chrono::system_clock::now()});

        // Before tick, events should not be dispatched to JS
        auto rBeforeTick = evalScript("globalThis._watchEvents.length;");
        CHECK(!rBeforeTick.thrown && ev::toDouble(rBeforeTick.value) == 0.0);

        // Tick
        broapps::api::tickAppsAsync();

        // After tick, both events should have arrived
        auto rAfterTick = evalScript(
            "(function() {\n"
            "  if (globalThis._watchEvents.length !== 2) return false;\n"
            "  const ev1 = globalThis._watchEvents[0];\n"
            "  if (ev1.type !== 'added' || ev1.appId !== 'test-app-2.desktop') return false;\n"
            "  const ev2 = globalThis._watchEvents[1];\n"
            "  if (ev2.type !== 'modified' || ev2.appId !== 'test-bro-calc.desktop') return false;\n"
            "  return true;\n"
            "})()\n"
        );
        CHECK(!rAfterTick.thrown);
        CHECK(ev::isBool(rAfterTick.value) && ev::toBool(rAfterTick.value));

        // Unwatch
        evalScript("globalThis._watchHandle.unwatch();");

        // Push another event and tick
        watcher->events().push(broapps::AppRemoved{"test-app-2.desktop", std::chrono::system_clock::now()});
        broapps::api::tickAppsAsync();

        // Events count should still be 2
        auto rAfterUnwatch = evalScript("globalThis._watchEvents.length;");
        CHECK(!rAfterUnwatch.thrown && ev::toDouble(rAfterUnwatch.value) == 2.0);
        std::cout << "  watch, unwatch, and tickAppsAsync [PASS]" << std::endl;
    }

    // 10. Shutdown and cleanup
    broapps::api::shutdownAppsAsync();
    std::filesystem::remove_all(tmp_dir);
    if (orig_config_home) {
        setenv("XDG_CONFIG_HOME", orig_config_str.c_str(), 1);
    } else {
        unsetenv("XDG_CONFIG_HOME");
    }
    if (orig_data_home) {
        setenv("XDG_DATA_HOME", orig_data_str.c_str(), 1);
    } else {
        unsetenv("XDG_DATA_HOME");
    }

    std::cout << "All broapps API tests PASSED!" << std::endl;
    return 0;
}
