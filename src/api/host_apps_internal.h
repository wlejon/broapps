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

} // namespace broapps::api
