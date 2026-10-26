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

// ---- Access gates ---------------------------------------------------------------
//
// Whether a program may start other programs, or change the user's desktop
// configuration, is the host's decision, not broapps's: the host knows which app
// is running and what it was granted. With no gate set (the default), everything
// is allowed and nothing below changes.
//
// Each gate answers "" to allow, or the sentence the refusal carries (naming what
// the app lacks and what it asked for). A gate is called on the JS thread that
// made the call, synchronously, before any work is queued, so it may block to ask
// the user; it must be thread-safe. A refusal is an Error whose `message` is
// exactly the gate's sentence and whose `code` is "EACCES": thrown by a
// synchronous native, the promise rejected with it by launch.

/// `what` describes the program to start, as one line: "launch <id or path>",
/// followed by the launch's arguments (an argument holding whitespace or nothing
/// is quoted). Asked by bro.apps.launch, the one native that starts a program.
using SpawnGate = std::function<std::string(const std::string& what)>;

/// Installs `gate` process-wide (an empty function removes it).
void setSpawnGate(SpawnGate gate);

/// `what` describes the change to the user's desktop configuration:
///   setDefaultApp / setDefaultAppForMime  "set the default app for <mime> to <appId>"
///   addRecent                             "add <path> to the recent files"
///   clearRecent                           "clear the recent files"
using WriteGate = std::function<std::string(const std::string& what)>;

/// Installs `gate` process-wide (an empty function removes it).
void setConfigWriteGate(WriteGate gate);

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
using broapps::api::setSpawnGate;
using broapps::api::setConfigWriteGate;
