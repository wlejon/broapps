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
