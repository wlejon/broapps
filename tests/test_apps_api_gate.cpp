// bro.apps under the host's access gate (embed/embed_access.h; api.h, "Access"):
// launch asks Spawn, setDefaultApp / setDefaultAppForMime / addRecent /
// clearRecent ask DesktopWrite, getRecent / getDefaultApp DesktopRead, and a
// file's path given to getAppsForMime Read; a refusal is an Error carrying the
// gate's sentence and code EACCES with nothing done, and with no gate everything
// behaves as before.
//
// Every service is a fake recording what it was asked to do, so the test runs on
// every platform and never launches a program or touches the account's recent
// files or file associations.

#include "../src/api/api.h"
#include "embed/embed.h"
#include "embed/embed_access.h"
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
    int sets = 0, defaultReads = 0, fileReads = 0;
    std::optional<broapps::AppInfo> get_default_app_for_mime(std::string_view) override {
        ++defaultReads;
        return std::nullopt;
    }
    std::optional<broapps::AppInfo> get_default_app_for_file(const std::filesystem::path&) override {
        ++fileReads;
        return std::nullopt;
    }
    bool set_default_app_for_mime(std::string_view, std::string_view) override {
        ++sets;
        return true;
    }
    std::vector<broapps::AppInfo> get_candidates_for_mime(std::string_view) override { return {}; }
    std::vector<broapps::AppInfo> get_candidates_for_file(const std::filesystem::path&) override {
        ++fileReads;
        return {};
    }
    std::string extension_to_mime(std::string_view) const override { return {}; }
    std::vector<std::string> mime_to_extensions(std::string_view) const override { return {}; }
    std::string mime_for_file(const std::filesystem::path&) const override { return {}; }
};

class FakeRecent : public broapps::RecentService {
public:
    int adds = 0, clears = 0, reads = 0;
    std::vector<broapps::RecentItem> get_recent_items(size_t) override {
        ++reads;
        return {};
    }
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
    std::vector<std::string> spawnAsked, writeAsked, readAsked, pathAsked;
    ev::setAccessGate([&](const ev::AccessRequest& r) -> std::string {
        std::lock_guard lock(mu);
        const std::string what(r.subject);
        switch (r.kind) {
            case ev::AccessKind::Spawn:
                spawnAsked.push_back(what);
                return what.find("Forbidden") != std::string::npos ? "no spawn permission: " + what : "";
            case ev::AccessKind::DesktopWrite:
                writeAsked.push_back(what);
                return "no files permission: " + what;
            case ev::AccessKind::DesktopRead:
                readAsked.push_back(what);
                return "no files permission: " + what;
            case ev::AccessKind::Read:
                pathAsked.push_back(what);
                return what.find("secret") != std::string::npos ? "no files access: " + what : "";
            default:
                return "unexpected access: " + what;
        }
    });

    // launch: refused before anything is queued; allowed launches still run.
    TEST_CHECK_EQ(awaitJs("bro.apps.launch('org.example.Forbidden', { args: ['--x', 'a b'] })"),
                  std::string("denied|no spawn permission: org.example.Forbidden --x \"a b\"|EACCES"));
    TEST_CHECK_EQ(awaitJs("bro.apps.launch({ exec: 'C:/Forbidden/tool.exe' })"),
                  std::string("denied|no spawn permission: C:/Forbidden/tool.exe|EACCES"));
    TEST_CHECK_EQ(launcher->launches.load(), 0);
    TEST_CHECK_EQ(awaitJs("bro.apps.launch('org.example.Editor')"), std::string("ok:handle"));
    TEST_CHECK_EQ(launcher->launches.load(), 1);
    TEST_CHECK_EQ(spawnAsked.back(), std::string("org.example.Editor"));

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
    TEST_CHECK_EQ(writeAsked.size(), size_t{4});

    // Reading it: the recent files and the default apps asked DesktopRead,
    // refused, nothing read.
    TEST_CHECK_EQ(syncJs("bro.apps.getRecent().length"),
                  std::string("denied|no files permission: read the recent files|EACCES"));
    TEST_CHECK_EQ(syncJs("bro.apps.getDefaultApp('text/plain')"),
                  std::string("denied|no files permission: read the default app for text/plain|EACCES"));
    TEST_CHECK_EQ(recent->reads, 0);
    TEST_CHECK_EQ(mime->defaultReads, 0);
    TEST_CHECK_EQ(readAsked.size(), size_t{2});

    // A file's path handed to getAppsForMime is read (sniffed): Read asked of
    // its absolute path; refused, the error carries it and nothing is read.
    TEST_CHECK_EQ(evalString("(function() { try { bro.apps.getAppsForMime('C:/secret/notes.txt'); return 'ok'; }\n"
                             "  catch (e) { return e.code + '|' + (e.path.indexOf('secret') >= 0) + '|' +\n"
                             "    (e.message === 'no files access: ' + e.path); } })()"),
                  std::string("EACCES|true|true"));
    TEST_CHECK_EQ(mime->fileReads, 0);
    TEST_CHECK_EQ(pathAsked.size(), size_t{1});
    TEST_CHECK_EQ(syncJs("bro.apps.getAppsForMime('C:/public/notes.txt').length"), std::string("ok:0"));
    TEST_CHECK_EQ(mime->fileReads, 1);
    // A MIME type is not a path: no Read asked.
    TEST_CHECK_EQ(syncJs("bro.apps.getAppsForMime('text/plain').length"), std::string("ok:0"));
    TEST_CHECK_EQ(pathAsked.size(), size_t{2});

    // No gate: everything as before, and nobody is asked.
    ev::setAccessGate(nullptr);
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
    TEST_CHECK_EQ(syncJs("bro.apps.getRecent().length"), std::string("ok:0"));
    TEST_CHECK_EQ(recent->reads, 1);
    TEST_CHECK_EQ(spawnAsked.size(), spawnBefore);
    TEST_CHECK_EQ(writeAsked.size(), size_t{4});
    TEST_CHECK_EQ(readAsked.size(), size_t{2});

    broapps::api::shutdownAppsAsync();
    std::cout << "broapps access gate test PASSED" << std::endl;
    return 0;
}
