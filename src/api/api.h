#pragma once

#include <functional>
#include <memory>
#include <string>

namespace broapps {
class AppCatalog;
class AppLauncher;
class IconResolver;
class MimeService;
class RecentService;
class CatalogWatcher;
}

namespace broapps::api {

/// Mounts `bro.apps` in the current Bronze realm.
void installApps();

/// Pumps async catalog change events and launcher events on the JS thread.
void tickAppsAsync();

/// Cleans up active watchers.
void shutdownAppsAsync();

/// Sets the catalog used by the API (defaults to AppCatalog::create()).
void setCatalog(std::shared_ptr<broapps::AppCatalog> catalog);

/// Gets the catalog currently used by the API.
std::shared_ptr<broapps::AppCatalog> getCatalog();

/// Sets the launcher used by the API.
void setLauncher(std::shared_ptr<broapps::AppLauncher> launcher);

/// Gets the launcher currently used by the API.
std::shared_ptr<broapps::AppLauncher> getLauncher();

/// Sets the icon resolver used by the API.
void setIconResolver(std::shared_ptr<broapps::IconResolver> resolver);

/// Gets the icon resolver currently used by the API.
std::shared_ptr<broapps::IconResolver> getIconResolver();

/// Sets the MIME service used by the API.
void setMimeService(std::shared_ptr<broapps::MimeService> mimeService);

/// Gets the MIME service currently used by the API.
std::shared_ptr<broapps::MimeService> getMimeService();

/// Sets the recent service used by the API.
void setRecentService(std::shared_ptr<broapps::RecentService> recentService);

/// Gets the recent service currently used by the API.
std::shared_ptr<broapps::RecentService> getRecentService();

/// Sets the catalog watcher used by the API.
void setCatalogWatcher(std::shared_ptr<broapps::CatalogWatcher> watcher);

/// Gets the catalog watcher currently used by the API.
std::shared_ptr<broapps::CatalogWatcher> getCatalogWatcher();

} // namespace broapps::api

using broapps::api::installApps;
using broapps::api::tickAppsAsync;
using broapps::api::shutdownAppsAsync;
using broapps::api::setCatalog;
using broapps::api::getCatalog;
using broapps::api::setLauncher;
using broapps::api::getLauncher;
using broapps::api::setIconResolver;
using broapps::api::getIconResolver;
using broapps::api::setMimeService;
using broapps::api::getMimeService;
using broapps::api::setRecentService;
using broapps::api::getRecentService;
using broapps::api::setCatalogWatcher;
using broapps::api::getCatalogWatcher;
