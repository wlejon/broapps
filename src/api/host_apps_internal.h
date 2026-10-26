#pragma once

#include "embed/embed.h"
#include "broapps/broapps.h"

#include <memory>
#include <string>
#include <vector>

namespace broapps::api {

namespace ev = bronze::embed;
using Value = bronze::Value;

// Service accessors
std::shared_ptr<broapps::AppCatalog> activeCatalog();
std::shared_ptr<broapps::AppLauncher> activeLauncher();
std::shared_ptr<broapps::IconResolver> activeIconResolver();
std::shared_ptr<broapps::MimeService> activeMimeService();
std::shared_ptr<broapps::RecentService> activeRecentService();
std::shared_ptr<broapps::CatalogWatcher> activeCatalogWatcher();
// The catalog watcher if one exists, else null: never builds a catalog. For
// the per-frame drain and teardown, which run in every app whether or not it
// uses bro.apps (building the catalog walks the Start Menu or the desktop
// entry dirs, hundreds of ms on the page's thread).
std::shared_ptr<broapps::CatalogWatcher> existingCatalogWatcher();

// The default catalog is built on a thread of its own (api.cpp).
// catalogReady() -> Promise<void> that resolves once it is (bro.apps.ready);
// catalogIsReady() says whether a call would answer without waiting;
// drainCatalogReady() resolves the waiting promises, on the page's thread.
Value catalogReady();
bool catalogIsReady();
void drainCatalogReady();

// The access gates' helpers (access_gate.cpp; the gates themselves are in api.h).
/// The spawn gate's answer for `what`: "" when allowed.
std::string spawnRefusal(const std::string& what);
/// The config write gate's answer for `what`: "" when allowed.
std::string configWriteRefusal(const std::string& what);
/// "launch <id>" and the arguments, as the spawn gate is asked.
std::string launchDescription(const std::string& id, const std::vector<std::string>& args);
/// The refusal as an Error: message and code "EACCES". Fresh: root it.
Value newAccessError(const std::string& refusal);
/// Throws newAccessError into the running program.
Value throwAccessError(const std::string& refusal);

// Conversions
Value makeError(const std::string& msg);
Value appInfoToJs(const broapps::AppInfo& app);
Value recentItemToJs(const broapps::RecentItem& item);

// ProcessHandle wrapping
void installProcessOnto(Value appsObj);
Value wrapProcessHandle(std::shared_ptr<broapps::ProcessHandle> handle);

// Apps installation onto bro.apps
void installAppsOnto(Value appsObj);
void drainWatcherEvents();
void clearWatchers();
void drainLaunchJobs();
void clearLaunchJobs();

} // namespace broapps::api
