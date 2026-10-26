// bro.apps under its access gates (api.h, "Access gates"): launch asks the spawn
// gate, setDefaultApp / setDefaultAppForMime / addRecent / clearRecent ask the
// config write gate, a refusal is an Error carrying the gate's sentence and code
// EACCES with nothing done, and with no gate everything behaves as before.
//
// Every service is a fake recording what it was asked to do, so the test runs on
// every platform and never launches a program or touches the account's recent
// files or file associations.

#include "../src/api/api.h"
#include "embed/embed.h"
#include "eval/eval.h"
#include "broapps/broapps.h"
#include "test_common.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace ev = bronze::embed;

namespace {

broapps::AppInfo fakeApp(std::string_view id) {
    broapps::AppInfo app;
    app.id = std::string(id);
    app.name = "Fake " + app.id;
    return app;
}

class FakeCatalog : public broapps::AppCatalog {
public:
    FakeCatalog() { apps_.push_back(fakeApp("org.example.Editor")); }
    const std::vector<broapps::AppInfo>& apps() const override { return apps_; }
    std::optional<broapps::AppInfo> find_by_id(std::string_view id) const override {
        for (const auto& a : apps_) {
            if (a.id == id) return a;
        }
        return std::nullopt;
    }
    std::vector<broapps::AppInfo> search(std::string_view) const override { return apps_; }
    std::vector<broapps::AppInfo> find_by_category(std::string_view) const override { return {}; }
    std::vector<broapps::AppInfo> find_by_mime_type(std::string_view) const override { return {}; }
    void refresh() override {}
    std::vector<std::filesystem::path> source_directories() const override { return {}; }

private:
    std::vector<broapps::AppInfo> apps_;
};

class FakeProcess : public broapps::ProcessHandle {
public:
    uint64_t launch_id() const override { return 1; }
    int64_t pid() const override { return 4242; }
    const std::string& scope_id() const override { return scope_; }
    bool is_running() const override { return false; }
    bool terminate() override { return true; }
    bool kill() override { return true; }
    bool wait_for_exit(std::chrono::milliseconds) override { return true; }
    std::optional<int> exit_code() const override { return 0; }

private:
    std::string scope_;
};

class FakeLauncher : public broapps::AppLauncher {
public:
    std::atomic<int> launches{0};
    broapps::MessageQueue<broapps::LaunchEvent>& events() override { return events_; }
    std::shared_ptr<broapps::ProcessHandle> launch(const broapps::AppInfo&, const broapps::LaunchScope&) override {
        ++launches;
        return std::make_shared<FakeProcess>();
    }
    std::shared_ptr<broapps::ProcessHandle> launch_executable(const std::string&,
                                                              const broapps::LaunchScope&) override {
        ++launches;
        return std::make_shared<FakeProcess>();
    }

private:
    broapps::MessageQueue<broapps::LaunchEvent> events_;
};

class FakeMime : public broapps::MimeService {
public:
    int sets = 0;
    std::optional<broapps::AppInfo> get_default_app_for_mime(std::string_view) override { return std::nullopt; }
    std::optional<broapps::AppInfo> get_default_app_for_file(const std::filesystem::path&) override {
        return std::nullopt;
    }
    bool set_default_app_for_mime(std::string_view, std::string_view) override {
        ++sets;
        return true;
    }
    std::vector<broapps::AppInfo> get_candidates_for_mime(std::string_view) override { return {}; }
    std::vector<broapps::AppInfo> get_candidates_for_file(const std::filesystem::path&) override { return {}; }
    std::string extension_to_mime(std::string_view) const override { return {}; }
    std::vector<std::string> mime_to_extensions(std::string_view) const override { return {}; }
    std::string mime_for_file(const std::filesystem::path&) const override { return {}; }
};

class FakeRecent : public broapps::RecentService {
public:
    int adds = 0, clears = 0;
    std::vector<broapps::RecentItem> get_recent_items(size_t) override { return {}; }
    bool add_recent_item(const std::filesystem::path&, const std::string&) override {
        ++adds;
        return true;
    }
    bool clear_recent_items() override {
        ++clears;
        return true;
    }
};

std::string evalString(const std::string& src) {
    auto r = bronze::eval::evalScript(src);
    if (r.thrown) return "<thrown>";
    return ev::isString(r.value) ? ev::toUtf8(r.value) : "<not a string>";
}

std::string syncJs(const std::string& expr) {
    return evalString("(function() { try { return 'ok:' + String(" + expr + "); }\n"
                      "  catch (e) { return 'denied|' + e.message + '|' + e.code; } })()");
}

std::string awaitJs(const std::string& expr) {
    bronze::eval::evalScript("globalThis._gateOut = undefined;\n"
                             "Promise.resolve().then(() => " + expr + ").then(\n"
                             "  v => 'ok:' + (v && typeof v === 'object' ? 'handle' : String(v)),\n"
                             "  e => 'denied|' + e.message + '|' + e.code)\n"
                             ".then(s => { globalThis._gateOut = s; });");
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::seconds(10)) {
        broapps::api::tickAppsAsync();
        ev::drainMicrotasks();
        std::string s = evalString("typeof globalThis._gateOut === 'string' ? globalThis._gateOut : ''");
        if (!s.empty()) return s;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return "<timeout>";
}

} // namespace

int main() {
    std::cout << "Starting broapps access gate test..." << std::endl;

    auto launcher = std::make_shared<FakeLauncher>();
    auto mime = std::make_shared<FakeMime>();
    auto recent = std::make_shared<FakeRecent>();
    broapps::api::setCatalog(std::make_shared<FakeCatalog>());
    broapps::api::setLauncher(launcher);
    broapps::api::setMimeService(mime);
    broapps::api::setRecentService(recent);
    broapps::api::installApps();

    std::mutex mu;
    std::vector<std::string> spawnAsked, writeAsked;
    broapps::api::setSpawnGate([&](const std::string& what) -> std::string {
        std::lock_guard lock(mu);
        spawnAsked.push_back(what);
        return what.find("Forbidden") != std::string::npos ? "no spawn permission: " + what : "";
    });
    broapps::api::setConfigWriteGate([&](const std::string& what) -> std::string {
        std::lock_guard lock(mu);
        writeAsked.push_back(what);
        return "no files permission: " + what;
    });

    // launch: refused before anything is queued; allowed launches still run.
    TEST_CHECK_EQ(awaitJs("bro.apps.launch('org.example.Forbidden', { args: ['--x', 'a b'] })"),
                  std::string("denied|no spawn permission: launch org.example.Forbidden --x \"a b\"|EACCES"));
    TEST_CHECK_EQ(awaitJs("bro.apps.launch({ exec: 'C:/Forbidden/tool.exe' })"),
                  std::string("denied|no spawn permission: launch C:/Forbidden/tool.exe|EACCES"));
    TEST_CHECK_EQ(launcher->launches.load(), 0);
    TEST_CHECK_EQ(awaitJs("bro.apps.launch('org.example.Editor')"), std::string("ok:handle"));
    TEST_CHECK_EQ(launcher->launches.load(), 1);
    TEST_CHECK_EQ(spawnAsked.back(), std::string("launch org.example.Editor"));

    // The desktop configuration: every write refused, nothing written.
    TEST_CHECK_EQ(syncJs("bro.apps.setDefaultApp('text/plain', 'org.example.Editor')"),
                  std::string("denied|no files permission: set the default app for text/plain to "
                              "org.example.Editor|EACCES"));
    TEST_CHECK_EQ(syncJs("bro.apps.setDefaultAppForMime('image/png', 'org.example.Editor')"),
                  std::string("denied|no files permission: set the default app for image/png to "
                              "org.example.Editor|EACCES"));
    TEST_CHECK_EQ(syncJs("bro.apps.addRecent('C:/notes.txt')"),
                  std::string("denied|no files permission: add C:/notes.txt to the recent files|EACCES"));
    TEST_CHECK_EQ(syncJs("bro.apps.clearRecent()"),
                  std::string("denied|no files permission: clear the recent files|EACCES"));
    TEST_CHECK_EQ(mime->sets, 0);
    TEST_CHECK_EQ(recent->adds, 0);
    TEST_CHECK_EQ(recent->clears, 0);
    // Reading the configuration asks nothing.
    TEST_CHECK_EQ(syncJs("bro.apps.getRecent().length"), std::string("ok:0"));
    TEST_CHECK_EQ(writeAsked.size(), size_t{4});

    // No gates: everything as before, and nobody is asked.
    broapps::api::setSpawnGate(nullptr);
    broapps::api::setConfigWriteGate(nullptr);
    const size_t spawnBefore = spawnAsked.size();
    TEST_CHECK_EQ(awaitJs("bro.apps.launch('org.example.Forbidden')"),
                  std::string("denied|App not found: org.example.Forbidden|undefined"));
    TEST_CHECK_EQ(awaitJs("bro.apps.launch({ exec: 'C:/Forbidden/tool.exe' })"), std::string("ok:handle"));
    TEST_CHECK_EQ(launcher->launches.load(), 2);
    TEST_CHECK_EQ(syncJs("bro.apps.setDefaultApp('text/plain', 'org.example.Editor')"), std::string("ok:true"));
    TEST_CHECK_EQ(syncJs("bro.apps.addRecent('C:/notes.txt')"), std::string("ok:true"));
    TEST_CHECK_EQ(syncJs("bro.apps.clearRecent()"), std::string("ok:true"));
    TEST_CHECK_EQ(mime->sets, 1);
    TEST_CHECK_EQ(recent->adds, 1);
    TEST_CHECK_EQ(recent->clears, 1);
    TEST_CHECK_EQ(spawnAsked.size(), spawnBefore);
    TEST_CHECK_EQ(writeAsked.size(), size_t{4});

    broapps::api::shutdownAppsAsync();
    std::cout << "broapps access gate test PASSED" << std::endl;
    return 0;
}
