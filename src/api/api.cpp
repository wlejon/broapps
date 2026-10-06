#include "api.h"
#include "host_apps_internal.h"
#include "broapps/broapps.h"

#include <mutex>

namespace broapps::api {

namespace {

std::mutex g_services_mu;

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

} // namespace

std::shared_ptr<broapps::AppCatalog> activeCatalog() {
    std::lock_guard lock(g_services_mu);
    if (g_custom_catalog) return g_custom_catalog;
    if (!g_default_catalog) {
        g_default_catalog = broapps::AppCatalog::create();
    }
    return g_default_catalog;
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
    std::lock_guard lock(g_services_mu);
    if (g_custom_mime_service) return g_custom_mime_service;
    if (!g_default_mime_service) {
        auto cat = g_custom_catalog ? g_custom_catalog : g_default_catalog;
        if (!cat) {
            g_default_catalog = broapps::AppCatalog::create();
            cat = g_default_catalog;
        }
        g_default_mime_service = broapps::MimeService::create(cat);
    }
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
    std::lock_guard lock(g_services_mu);
    if (g_custom_catalog_watcher) return g_custom_catalog_watcher;
    if (!g_default_catalog_watcher) {
        auto cat = g_custom_catalog ? g_custom_catalog : g_default_catalog;
        if (!cat) {
            g_default_catalog = broapps::AppCatalog::create();
            cat = g_default_catalog;
        }
        g_default_catalog_watcher = broapps::CatalogWatcher::create(cat);
    }
    return g_default_catalog_watcher;
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
    drainWatcherEvents();
}

void shutdownAppsAsync() {
    clearWatchers();
}

} // namespace broapps::api
