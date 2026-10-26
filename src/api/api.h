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

// ---- Access -----------------------------------------------------------------------
//
// Whether a program may start other programs, or read or change the user's
// desktop configuration, is the host's decision, not broapps's: the natives below
// ask the host's one access gate, bronze::embed::checkAccess
// (embed/embed_access.h), on the JS thread that made the call, synchronously,
// before any work is queued. With no gate installed everything is allowed and
// nothing below changes.
//
//   launch                 Spawn "<id or path>" and the launch's arguments, one
//                          line (an argument holding whitespace or nothing is
//                          quoted): the one native that starts a program
//   getRecent              DesktopRead "read the recent files"
//   getDefaultApp          DesktopRead "read the default app for <mime or path>"
//   setDefaultApp / setDefaultAppForMime
//                          DesktopWrite "set the default app for <mime> to <appId>"
//   addRecent              DesktopWrite "add <path> to the recent files"
//   clearRecent            DesktopWrite "clear the recent files"
//   getDefaultApp / getAppsForMime given a file's path (anything but a
//                          "type/subtype" MIME type): Read of the path, which is
//                          sniffed
//
// A refusal is bronze::embed::newAccessError: an Error whose `message` is the
// gate's sentence (named PermissionRevoked for a grant taken back), `code` is
// "EACCES" and, for a path, `path`: thrown by a synchronous native, launch's
// promise rejected with it.

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
