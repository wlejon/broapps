#include "api.h"
#include "host_apps_internal.h"
#include "broapps/broapps.h"

#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

namespace broapps::api {

namespace {

std::mutex g_services_mu;

// The default catalog (and the MIME service over it) is built on a thread of
// its own: reading every desktop entry / Start Menu shortcut and the
// registry's associations takes hundreds of milliseconds. bro.apps.ready()
// starts it and resolves when it is done; a synchronous call made before
// then waits for that build rather than starting a second one.
std::mutex g_build_mu;
std::condition_variable g_build_cv;
bool g_build_started = false;
bool g_build_done = false;
std::vector<std::shared_ptr<ev::Persistent>> g_ready_promises;  // the page's thread only

std::shared_ptr<broapps::AppCatalog> g_custom_catalog;
std::shared_ptr<broapps::AppCatalog> g_default_catalog;

std::shared_ptr<broapps::AppLauncher> g_custom_launcher;
std::shared_ptr<broapps::AppLauncher> g_default_launcher;

std::shared_ptr<broapps::IconResolver> g_custom_icon_resolver;
std::shared_ptr<broapps::IconResolver> g_default_icon_resolver;

std::shared_ptr<broapps::MimeService> g_custom_mime_service;
std::shared_ptr<broapps::MimeService> g_default_mime_service;

std::shared_ptr<broapps::RecentService> g_custom_recent_service;
std::shared_ptr<broapps::RecentService> g_default_recent_service;

std::shared_ptr<broapps::CatalogWatcher> g_custom_catalog_watcher;
std::shared_ptr<broapps::CatalogWatcher> g_default_catalog_watcher;

// The build itself, on the build thread: the catalog, then the MIME service
// over it, published together.
void buildDefaults() {
    std::shared_ptr<broapps::AppCatalog> catalog;
    std::shared_ptr<broapps::MimeService> mime;
    try {
        catalog = broapps::AppCatalog::create();
        if (catalog) mime = broapps::MimeService::create(catalog);
    } catch (...) {
        // Left null: the accessors answer with nothing, as for no catalog.
    }
    {
        std::lock_guard lock(g_services_mu);
        if (!g_default_catalog) g_default_catalog = catalog;
        if (!g_default_mime_service && g_default_catalog == catalog) g_default_mime_service = mime;
    }
    {
        std::lock_guard lock(g_build_mu);
        g_build_done = true;
    }
    g_build_cv.notify_all();
}

// Starts the build once. True when it has already finished.
bool startDefaultBuild() {
    std::lock_guard lock(g_build_mu);
    if (g_build_done) return true;
    if (!g_build_started) {
        g_build_started = true;
        // Detached: g_build_done (under g_build_mu) is how it is waited for,
        // and shutdownAppsAsync waits for it before teardown.
        std::thread(buildDefaults).detach();
    }
    return false;
}

// The default catalog and MIME service, built (waiting for the build when
// it is in flight).
void ensureDefaults() {
    if (startDefaultBuild()) return;
    std::unique_lock lock(g_build_mu);
    g_build_cv.wait(lock, [] { return g_build_done; });
}

bool customCatalogSet() {
    std::lock_guard lock(g_services_mu);
    return g_custom_catalog != nullptr;
}

} // namespace

std::shared_ptr<broapps::AppCatalog> activeCatalog() {
    {
        std::lock_guard lock(g_services_mu);
        if (g_custom_catalog) return g_custom_catalog;
        if (g_default_catalog) return g_default_catalog;
    }
    ensureDefaults();
    std::lock_guard lock(g_services_mu);
    if (g_custom_catalog) return g_custom_catalog;
    if (!g_default_catalog) g_default_catalog = broapps::AppCatalog::create();  // the build failed
    return g_default_catalog;
}

Value catalogReady() {
    ev::Persistent promise(ev::createPromise());
    if (customCatalogSet() || startDefaultBuild()) {
        ev::resolvePromise(promise.get(), ev::undefined());
        return promise.get();
    }
    g_ready_promises.push_back(std::make_shared<ev::Persistent>(promise.get()));
    return promise.get();
}

bool catalogIsReady() {
    if (customCatalogSet()) return true;
    std::lock_guard lock(g_build_mu);
    return g_build_done;
}

void drainCatalogReady() {
    if (g_ready_promises.empty() || !catalogIsReady()) return;
    auto promises = std::move(g_ready_promises);
    g_ready_promises.clear();
    for (auto& p : promises) ev::resolvePromise(p->get(), ev::undefined());
}

void setCatalog(std::shared_ptr<broapps::AppCatalog> catalog) {
    std::lock_guard lock(g_services_mu);
    g_custom_catalog = std::move(catalog);
    g_default_mime_service.reset();
    g_default_catalog_watcher.reset();
}

std::shared_ptr<broapps::AppCatalog> getCatalog() {
    return activeCatalog();
}

std::shared_ptr<broapps::AppLauncher> activeLauncher() {
    std::lock_guard lock(g_services_mu);
    if (g_custom_launcher) return g_custom_launcher;
    if (!g_default_launcher) {
        g_default_launcher = broapps::AppLauncher::create();
    }
    return g_default_launcher;
}

void setLauncher(std::shared_ptr<broapps::AppLauncher> launcher) {
    std::lock_guard lock(g_services_mu);
    g_custom_launcher = std::move(launcher);
}

std::shared_ptr<broapps::AppLauncher> getLauncher() {
    return activeLauncher();
}

std::shared_ptr<broapps::IconResolver> activeIconResolver() {
    std::lock_guard lock(g_services_mu);
    if (g_custom_icon_resolver) return g_custom_icon_resolver;
    if (!g_default_icon_resolver) {
        g_default_icon_resolver = broapps::IconResolver::create();
    }
    return g_default_icon_resolver;
}

void setIconResolver(std::shared_ptr<broapps::IconResolver> resolver) {
    std::lock_guard lock(g_services_mu);
    g_custom_icon_resolver = std::move(resolver);
}

std::shared_ptr<broapps::IconResolver> getIconResolver() {
    return activeIconResolver();
}

std::shared_ptr<broapps::MimeService> activeMimeService() {
    {
        std::lock_guard lock(g_services_mu);
        if (g_custom_mime_service) return g_custom_mime_service;
        if (g_default_mime_service) return g_default_mime_service;
    }
    // Over the catalog in use: the default one comes with its MIME service
    // from the build; a catalog set by the host gets one made here.
    auto cat = activeCatalog();
    std::lock_guard lock(g_services_mu);
    if (g_custom_mime_service) return g_custom_mime_service;
    if (!g_default_mime_service && cat) g_default_mime_service = broapps::MimeService::create(cat);
    return g_default_mime_service;
}

void setMimeService(std::shared_ptr<broapps::MimeService> mimeService) {
    std::lock_guard lock(g_services_mu);
    g_custom_mime_service = std::move(mimeService);
}

std::shared_ptr<broapps::MimeService> getMimeService() {
    return activeMimeService();
}

std::shared_ptr<broapps::RecentService> activeRecentService() {
    std::lock_guard lock(g_services_mu);
    if (g_custom_recent_service) return g_custom_recent_service;
    if (!g_default_recent_service) {
        g_default_recent_service = broapps::RecentService::create();
    }
    return g_default_recent_service;
}

void setRecentService(std::shared_ptr<broapps::RecentService> recentService) {
    std::lock_guard lock(g_services_mu);
    g_custom_recent_service = std::move(recentService);
}

std::shared_ptr<broapps::RecentService> getRecentService() {
    return activeRecentService();
}

std::shared_ptr<broapps::CatalogWatcher> activeCatalogWatcher() {
    {
        std::lock_guard lock(g_services_mu);
        if (g_custom_catalog_watcher) return g_custom_catalog_watcher;
        if (g_default_catalog_watcher) return g_default_catalog_watcher;
    }
    auto cat = activeCatalog();
    std::lock_guard lock(g_services_mu);
    if (g_custom_catalog_watcher) return g_custom_catalog_watcher;
    if (!g_default_catalog_watcher && cat) g_default_catalog_watcher = broapps::CatalogWatcher::create(cat);
    return g_default_catalog_watcher;
}

std::shared_ptr<broapps::CatalogWatcher> existingCatalogWatcher() {
    std::lock_guard lock(g_services_mu);
    return g_custom_catalog_watcher ? g_custom_catalog_watcher : g_default_catalog_watcher;
}

void setCatalogWatcher(std::shared_ptr<broapps::CatalogWatcher> watcher) {
    std::lock_guard lock(g_services_mu);
    g_custom_catalog_watcher = std::move(watcher);
}

std::shared_ptr<broapps::CatalogWatcher> getCatalogWatcher() {
    return activeCatalogWatcher();
}

Value ensureBroApps() {
    ev::Persistent globalThisVal;
    auto gt = ev::globalValue("globalThis");
    if (gt.found && ev::isObject(gt.value)) {
        globalThisVal.set(gt.value);
    }

    ev::Persistent broP;
    auto bro = ev::globalValue("bro");
    if (bro.found && ev::isObject(bro.value)) broP.set(bro.value);
    if (!ev::isObject(broP.get()) && ev::isObject(globalThisVal.get())) {
        Value candidate = ev::getProperty(globalThisVal.get(), "bro");
        if (ev::isObject(candidate)) broP.set(candidate);
    }
    if (!ev::isObject(broP.get())) {
        broP.set(ev::createObject());
        ev::registerGlobal("bro", broP.get());
        if (ev::isObject(globalThisVal.get())) {
            globalThisVal.set(ev::setProperty(globalThisVal.get(), "bro", broP.get()));
        }
    }

    ev::Persistent appsP(ev::getProperty(broP.get(), "apps"));
    if (!ev::isObject(appsP.get())) {
        appsP.set(ev::createObject());
        broP.set(ev::setProperty(broP.get(), "apps", appsP.get()));
    }
    return appsP.get();
}

void installApps() {
    ev::Persistent appsObj(ensureBroApps());
    installProcessOnto(appsObj.get());
    installAppsOnto(appsObj.get());
}

void tickAppsAsync() {
    drainCatalogReady();
    drainWatcherEvents();
    drainLaunchJobs();
}

void shutdownAppsAsync() {
    clearWatchers();
    clearLaunchJobs();
    g_ready_promises.clear();
    // A build still running finishes before teardown frees what it uses.
    {
        std::unique_lock lock(g_build_mu);
        g_build_cv.wait(lock, [] { return g_build_done || !g_build_started; });
    }
}

} // namespace broapps::api
