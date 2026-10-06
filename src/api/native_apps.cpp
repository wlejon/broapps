#include "host_apps_internal.h"
#include "arg_reader.h"
#include "object_builder.h"

#include <chrono>
#include <filesystem>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace broapps::api {

namespace {

struct ActiveWatcher {
    uint64_t token = 0;
    std::shared_ptr<ev::Persistent> callback;
};

std::mutex g_watcher_mu;
uint64_t g_next_watch_token = 1;
std::unordered_map<uint64_t, ActiveWatcher> g_watchers;

Value makeStringArray(const std::vector<std::string>& vec) {
    ev::Persistent arr(ev::makeArray(static_cast<uint32_t>(vec.size())));
    for (uint32_t i = 0; i < vec.size(); ++i) {
        ev::Persistent str(ev::fromUtf8(vec[i]));
        ev::setElement(arr.get(), i, str.get());
    }
    return arr.get();
}

std::vector<std::string> getObjectKeys(Value objVal) {
    std::vector<std::string> keys;
    if (!ev::isObject(objVal)) return keys;
    ev::Persistent obj(objVal);
    auto gObject = ev::globalValue("Object");
    if (!gObject.found || !ev::isObject(gObject.value)) return keys;
    ev::Persistent objCtor(gObject.value);
    ev::Persistent keysFn(ev::getProperty(objCtor.get(), "keys"));
    if (!ev::isFunction(keysFn.get())) return keys;
    Value arg = obj.get();
    auto kres = ev::call(keysFn.get(), objCtor.get(), std::span<const Value>(&arg, 1));
    if (kres.thrown || !ev::isObject(kres.value)) return keys;
    ev::Persistent keysArr(kres.value);
    Value lenVal = ev::getProperty(keysArr.get(), "length");
    uint32_t len = ev::isNumber(lenVal) ? static_cast<uint32_t>(ev::toDouble(lenVal)) : 0;
    for (uint32_t i = 0; i < len; ++i) {
        keys.push_back(ev::toUtf8(ev::getElement(keysArr.get(), i)));
    }
    return keys;
}

bool unwatchInternal(uint64_t token) {
    std::lock_guard lock(g_watcher_mu);
    auto it = g_watchers.find(token);
    if (it == g_watchers.end()) return false;
    g_watchers.erase(it);

    if (g_watchers.empty()) {
        auto w = activeCatalogWatcher();
        if (w && w->is_watching()) {
            w->stop();
        }
    }
    return true;
}

} // namespace

Value makeError(const std::string& msg) {
    ev::Persistent text(ev::fromUtf8(msg));
    auto ctor = ev::globalValue("Error");
    if (ctor.found && ev::isFunction(ctor.value)) {
        ev::Persistent c(ctor.value);
        const Value arg = text.get();
        auto r = ev::construct(c.get(), std::span<const Value>(&arg, 1));
        if (!r.thrown) return r.value;
    }
    return text.get();
}

Value appInfoToJs(const broapps::AppInfo& app) {
    ObjectBuilder b;
    b.set("id", app.id);
    b.set("name", app.name);
    b.set("generic_name", app.generic_name);
    b.set("genericName", app.generic_name);
    b.set("comment", app.comment);
    b.set("icon", app.icon_name_or_path);
    b.set("icon_name_or_path", app.icon_name_or_path);
    b.set("exec", app.executable_path);
    b.set("executable_path", app.executable_path);
    b.set("working_directory", app.working_directory);
    b.set("workingDirectory", app.working_directory);

    b.set("categories", makeStringArray(app.categories));
    b.set("keywords", makeStringArray(app.keywords));
    b.set("mime_types", makeStringArray(app.supported_mime_types));
    b.set("mimeTypes", makeStringArray(app.supported_mime_types));
    b.set("supported_protocols", makeStringArray(app.supported_protocols));

    b.set("terminal", app.is_terminal);
    b.set("is_terminal", app.is_terminal);
    b.set("nodisplay", app.is_nodisplay);
    b.set("is_nodisplay", app.is_nodisplay);
    b.set("packaged", app.is_packaged);
    b.set("is_packaged", app.is_packaged);

    if (!app.actions.empty()) {
        ev::Persistent actArr(ev::makeArray(static_cast<uint32_t>(app.actions.size())));
        for (uint32_t i = 0; i < app.actions.size(); ++i) {
            ObjectBuilder act;
            act.set("id", app.actions[i].id);
            act.set("name", app.actions[i].name);
            act.set("exec", app.actions[i].exec);
            act.set("icon", app.actions[i].icon);
            ev::setElement(actArr.get(), i, act.build());
        }
        b.set("actions", actArr.get());
    } else {
        b.set("actions", ev::makeArray(0));
    }

    if (!app.desktop_file_path.empty()) b.set("desktop_file_path", app.desktop_file_path);
    if (!app.exec_raw.empty()) b.set("exec_raw", app.exec_raw);
    if (!app.try_exec.empty()) b.set("try_exec", app.try_exec);
    if (!app.aumid.empty()) b.set("aumid", app.aumid);
    if (!app.shortcut_path.empty()) b.set("shortcut_path", app.shortcut_path);
    if (!app.arguments.empty()) b.set("arguments", app.arguments);
    if (!app.bundle_id.empty()) b.set("bundle_id", app.bundle_id);
    if (!app.bundle_path.empty()) b.set("bundle_path", app.bundle_path);

    return b.build();
}

Value recentItemToJs(const broapps::RecentItem& item) {
    ObjectBuilder b;
    b.set("filePath", item.file_path.string());
    b.set("file_path", item.file_path.string());
    b.set("uri", item.uri);
    b.set("displayName", item.display_name);
    b.set("display_name", item.display_name);
    b.set("mimeType", item.mime_type);
    b.set("mime_type", item.mime_type);
    b.set("appId", item.app_id);
    b.set("app_id", item.app_id);

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        item.timestamp.time_since_epoch()).count();
    b.set("timestamp", static_cast<double>(ms));
    return b.build();
}

void drainWatcherEvents() {
    auto watcher = activeCatalogWatcher();
    if (!watcher) return;

    auto events = watcher->events().drain();
    if (events.empty()) return;

    std::vector<std::shared_ptr<ev::Persistent>> callbacks;
    {
        std::lock_guard lock(g_watcher_mu);
        if (g_watchers.empty()) return;
        for (const auto& [token, w] : g_watchers) {
            callbacks.push_back(w.callback);
        }
    }

    for (const auto& evItem : events) {
        std::string type;
        std::string appId;
        std::chrono::system_clock::time_point tp;

        std::visit([&](const auto& e) {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, AppAdded>) {
                type = "added";
                appId = e.app_id;
                tp = e.timestamp;
            } else if constexpr (std::is_same_v<T, AppRemoved>) {
                type = "removed";
                appId = e.app_id;
                tp = e.timestamp;
            } else if constexpr (std::is_same_v<T, AppModified>) {
                type = "modified";
                appId = e.app_id;
                tp = e.timestamp;
            } else if constexpr (std::is_same_v<T, CatalogRefreshed>) {
                type = "refreshed";
                tp = e.timestamp;
            }
        }, evItem);

        ObjectBuilder eventObj;
        eventObj.set("type", type);
        eventObj.set("appId", appId);
        eventObj.set("app_id", appId);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
        eventObj.set("timestamp", static_cast<double>(ms));

        ev::Persistent jsEvent(eventObj.build());

        for (const auto& cb : callbacks) {
            if (cb && ev::isFunction(cb->get())) {
                Value arg = jsEvent.get();
                ev::call(cb->get(), ev::undefined(), std::span<const Value>(&arg, 1));
            }
        }
    }
}

void clearWatchers() {
    std::lock_guard lock(g_watcher_mu);
    g_watchers.clear();
    auto w = activeCatalogWatcher();
    if (w && w->is_watching()) {
        w->stop();
    }
}

void installAppsOnto(Value appsObj) {
    ObjectBuilder apps(appsObj);

    // bro.apps.list() -> AppInfo[]
    apps.def("list", 0, [](Value, std::span<const Value>) -> Value {
        auto catalog = activeCatalog();
        if (!catalog) return ev::makeArray(0);

        const auto& appList = catalog->apps();
        ev::Persistent arr(ev::makeArray(static_cast<uint32_t>(appList.size())));
        for (uint32_t i = 0; i < appList.size(); ++i) {
            ev::Persistent item(appInfoToJs(appList[i]));
            ev::setElement(arr.get(), i, item.get());
        }
        return arr.get();
    });

    // bro.apps.get(appId) -> AppInfo | null
    apps.def("get", 1, [](Value, std::span<const Value> args) -> Value {
        if (args.empty() || !ev::isString(args[0])) return ev::null();
        std::string id = ev::toUtf8(args[0]);
        auto catalog = activeCatalog();
        if (!catalog) return ev::null();

        auto app = catalog->find_by_id(id);
        if (app) {
            return appInfoToJs(*app);
        }
        return ev::null();
    });

    // bro.apps.search(query) -> AppInfo[]
    apps.def("search", 1, [](Value, std::span<const Value> args) -> Value {
        if (args.empty() || !ev::isString(args[0])) return ev::makeArray(0);
        std::string q = ev::toUtf8(args[0]);
        auto catalog = activeCatalog();
        if (!catalog) return ev::makeArray(0);

        auto results = catalog->search(q);
        ev::Persistent arr(ev::makeArray(static_cast<uint32_t>(results.size())));
        for (uint32_t i = 0; i < results.size(); ++i) {
            ev::Persistent item(appInfoToJs(results[i]));
            ev::setElement(arr.get(), i, item.get());
        }
        return arr.get();
    });

    // bro.apps.launch(appId, options?) -> Promise<ProcessHandle>
    apps.def("launch", 1, [](Value, std::span<const Value> args) -> Value {
        ev::Persistent promiseP(ev::createPromise());

        if (args.empty() || (!ev::isString(args[0]) && !ev::isObject(args[0]))) {
            ev::Persistent err(makeError("bro.apps.launch requires an app id or AppInfo"));
            ev::rejectPromise(promiseP.get(), err.get());
            return promiseP.get();
        }

        std::string id;
        if (ev::isString(args[0])) {
            id = ev::toUtf8(args[0]);
        } else if (ev::isObject(args[0])) {
            Value idVal = ev::getProperty(args[0], "id");
            if (ev::isString(idVal)) {
                id = ev::toUtf8(idVal);
            } else {
                Value execVal = ev::getProperty(args[0], "exec");
                if (ev::isString(execVal)) {
                    id = ev::toUtf8(execVal);
                }
            }
        }

        if (id.empty()) {
            ev::Persistent err(makeError("bro.apps.launch requires a valid app id"));
            ev::rejectPromise(promiseP.get(), err.get());
            return promiseP.get();
        }

        LaunchScope scope;
        if (args.size() > 1 && ev::isObject(args[1])) {
            ev::Persistent optObj(args[1]);

            // cwd
            Value cwdVal = ev::getProperty(optObj.get(), "cwd");
            if (ev::isString(cwdVal)) {
                scope.working_directory = ev::toUtf8(cwdVal);
            }

            // args
            Value argsVal = ev::getProperty(optObj.get(), "args");
            if (ev::isObject(argsVal)) {
                ev::Persistent argsArr(argsVal);
                Value lenVal = ev::getProperty(argsArr.get(), "length");
                if (ev::isNumber(lenVal)) {
                    uint32_t len = static_cast<uint32_t>(ev::toDouble(lenVal));
                    for (uint32_t i = 0; i < len; ++i) {
                        scope.arguments.push_back(ev::toUtf8(ev::getElement(argsArr.get(), i)));
                    }
                }
            }

            // files
            Value filesVal = ev::getProperty(optObj.get(), "files");
            if (ev::isObject(filesVal)) {
                ev::Persistent filesArr(filesVal);
                Value lenVal = ev::getProperty(filesArr.get(), "length");
                if (ev::isNumber(lenVal)) {
                    uint32_t len = static_cast<uint32_t>(ev::toDouble(lenVal));
                    for (uint32_t i = 0; i < len; ++i) {
                        scope.files_to_open.push_back(ev::toUtf8(ev::getElement(filesArr.get(), i)));
                    }
                }
            }

            // env
            Value envVal = ev::getProperty(optObj.get(), "env");
            if (ev::isObject(envVal)) {
                std::vector<std::string> keys = getObjectKeys(envVal);
                ev::Persistent envObj(envVal);
                for (const auto& k : keys) {
                    Value v = ev::getProperty(envObj.get(), k);
                    scope.environment.emplace_back(k, ev::toUtf8(v));
                }
            }

            // terminal
            Value termVal = ev::getProperty(optObj.get(), "terminal");
            if (ev::isBool(termVal)) {
                scope.terminal = ev::toBool(termVal);
            }

            // action / actionId
            Value actVal = ev::getProperty(optObj.get(), "action");
            if (ev::isString(actVal)) {
                scope.action_id = ev::toUtf8(actVal);
            } else {
                Value actIdVal = ev::getProperty(optObj.get(), "actionId");
                if (ev::isString(actIdVal)) scope.action_id = ev::toUtf8(actIdVal);
            }
        }

        try {
            auto catalog = activeCatalog();
            auto launcher = activeLauncher();
            if (!launcher) {
                ev::Persistent err(makeError("App launcher unavailable"));
                ev::rejectPromise(promiseP.get(), err.get());
                return promiseP.get();
            }

            std::optional<AppInfo> appOpt;
            if (catalog) {
                appOpt = catalog->find_by_id(id);
            }

            std::shared_ptr<ProcessHandle> handle;
            if (appOpt) {
                handle = launcher->launch(*appOpt, scope);
            } else {
                std::error_code ec;
                if (std::filesystem::exists(id, ec) || id.find('/') != std::string::npos || id.find('\\') != std::string::npos) {
                    handle = launcher->launch_executable(id, scope);
                }
            }

            if (handle) {
                ev::Persistent handleVal(wrapProcessHandle(std::move(handle)));
                ev::resolvePromise(promiseP.get(), handleVal.get());
            } else {
                ev::Persistent err(makeError("App not found: " + id));
                ev::rejectPromise(promiseP.get(), err.get());
            }
        } catch (const std::exception& ex) {
            ev::Persistent err(makeError(std::string("Launch failed: ") + ex.what()));
            ev::rejectPromise(promiseP.get(), err.get());
        }

        return promiseP.get();
    });

    // bro.apps.resolveIcon(iconName, options?) -> string | null
    apps.def("resolveIcon", 1, [](Value, std::span<const Value> args) -> Value {
        if (args.empty() || !ev::isString(args[0])) return ev::null();
        std::string iconName = ev::toUtf8(args[0]);
        if (iconName.empty()) return ev::null();

        uint32_t size = 48;
        if (args.size() > 1) {
            if (ev::isNumber(args[1])) {
                double d = ev::toDouble(args[1]);
                if (d > 0) size = static_cast<uint32_t>(d);
            } else if (ev::isObject(args[1])) {
                Value szVal = ev::getProperty(args[1], "size");
                if (ev::isNumber(szVal)) {
                    double d = ev::toDouble(szVal);
                    if (d > 0) size = static_cast<uint32_t>(d);
                }
            }
        }

        auto resolver = activeIconResolver();
        if (!resolver) return ev::null();

        auto path = resolver->resolve_icon(iconName, size);
        if (path) {
            return ev::fromUtf8(path->string());
        }
        return ev::null();
    });

    // bro.apps.getDefaultApp(mimeType) -> AppInfo | null
    apps.def("getDefaultApp", 1, [](Value, std::span<const Value> args) -> Value {
        if (args.empty() || !ev::isString(args[0])) return ev::null();
        std::string mimeType = ev::toUtf8(args[0]);
        auto mimeService = activeMimeService();
        if (!mimeService) return ev::null();

        auto app = mimeService->get_default_app_for_mime(mimeType);
        if (!app && (mimeType.find('/') == std::string::npos || std::filesystem::exists(mimeType))) {
            app = mimeService->get_default_app_for_file(mimeType);
        }
        if (app) {
            return appInfoToJs(*app);
        }
        return ev::null();
    });

    // bro.apps.setDefaultAppForMime(mimeType, appId) -> boolean
    apps.def("setDefaultAppForMime", 2, [](Value, std::span<const Value> args) -> Value {
        if (args.size() < 2 || !ev::isString(args[0]) || !ev::isString(args[1])) {
            return ev::fromBool(false);
        }
        std::string mimeType = ev::toUtf8(args[0]);
        std::string appId = ev::toUtf8(args[1]);
        auto mimeService = activeMimeService();
        if (!mimeService) return ev::fromBool(false);

        return ev::fromBool(mimeService->set_default_app_for_mime(mimeType, appId));
    });

    // bro.apps.setDefaultApp(mimeType, appId) -> boolean (alias)
    apps.def("setDefaultApp", 2, [](Value, std::span<const Value> args) -> Value {
        if (args.size() < 2 || !ev::isString(args[0]) || !ev::isString(args[1])) {
            return ev::fromBool(false);
        }
        std::string mimeType = ev::toUtf8(args[0]);
        std::string appId = ev::toUtf8(args[1]);
        auto mimeService = activeMimeService();
        if (!mimeService) return ev::fromBool(false);

        return ev::fromBool(mimeService->set_default_app_for_mime(mimeType, appId));
    });

    // bro.apps.getAppsForMime(mimeType) -> AppInfo[]
    apps.def("getAppsForMime", 1, [](Value, std::span<const Value> args) -> Value {
        if (args.empty() || !ev::isString(args[0])) return ev::makeArray(0);
        std::string mimeType = ev::toUtf8(args[0]);
        auto mimeService = activeMimeService();
        if (!mimeService) return ev::makeArray(0);

        auto appsList = mimeService->get_candidates_for_mime(mimeType);
        if (appsList.empty() && (mimeType.find('/') == std::string::npos || std::filesystem::exists(mimeType))) {
            appsList = mimeService->get_candidates_for_file(mimeType);
        }

        ev::Persistent arr(ev::makeArray(static_cast<uint32_t>(appsList.size())));
        for (uint32_t i = 0; i < appsList.size(); ++i) {
            ev::Persistent item(appInfoToJs(appsList[i]));
            ev::setElement(arr.get(), i, item.get());
        }
        return arr.get();
    });

    // bro.apps.getRecent(limit?) -> RecentItem[]
    apps.def("getRecent", 0, [](Value, std::span<const Value> args) -> Value {
        size_t limit = 50;
        if (!args.empty() && ev::isNumber(args[0])) {
            double d = ev::toDouble(args[0]);
            if (d > 0) limit = static_cast<size_t>(d);
        }

        auto recentService = activeRecentService();
        if (!recentService) return ev::makeArray(0);

        auto items = recentService->get_recent_items(limit);
        ev::Persistent arr(ev::makeArray(static_cast<uint32_t>(items.size())));
        for (uint32_t i = 0; i < items.size(); ++i) {
            ev::Persistent item(recentItemToJs(items[i]));
            ev::setElement(arr.get(), i, item.get());
        }
        return arr.get();
    });

    // bro.apps.addRecent(filePath, appId?) -> boolean
    apps.def("addRecent", 1, [](Value, std::span<const Value> args) -> Value {
        if (args.empty() || !ev::isString(args[0])) return ev::fromBool(false);
        std::string path = ev::toUtf8(args[0]);
        std::string appId;
        if (args.size() > 1 && ev::isString(args[1])) {
            appId = ev::toUtf8(args[1]);
        }
        auto recentService = activeRecentService();
        if (!recentService) return ev::fromBool(false);
        return ev::fromBool(recentService->add_recent_item(path, appId));
    });

    // bro.apps.clearRecent() -> boolean
    apps.def("clearRecent", 0, [](Value, std::span<const Value>) -> Value {
        auto recentService = activeRecentService();
        if (!recentService) return ev::fromBool(false);
        return ev::fromBool(recentService->clear_recent_items());
    });

    // bro.apps.watch(callback) -> WatchHandle
    apps.def("watch", 1, [](Value, std::span<const Value> args) -> Value {
        if (args.empty() || !ev::isFunction(args[0])) {
            return ev::throwTypeError("bro.apps.watch requires a callback function");
        }

        uint64_t token = 0;
        {
            std::lock_guard lock(g_watcher_mu);
            token = g_next_watch_token++;
            ActiveWatcher watcher;
            watcher.token = token;
            watcher.callback = std::make_shared<ev::Persistent>(args[0]);
            g_watchers.emplace(token, std::move(watcher));
        }

        auto w = activeCatalogWatcher();
        if (w && !w->is_watching()) {
            w->start();
        }

        ObjectBuilder handle;
        handle.set("token", static_cast<double>(token));
        handle.def("unwatch", 0, [token](Value, std::span<const Value>) -> Value {
            bool ok = unwatchInternal(token);
            return ev::fromBool(ok);
        });
        return handle.build();
    });

    // bro.apps.unwatch(handleOrToken) -> boolean
    apps.def("unwatch", 1, [](Value, std::span<const Value> args) -> Value {
        if (args.empty()) return ev::fromBool(false);
        uint64_t token = 0;
        if (ev::isObject(args[0])) {
            Value tokVal = ev::getProperty(args[0], "token");
            if (ev::isNumber(tokVal)) {
                token = static_cast<uint64_t>(ev::toDouble(tokVal));
            }
        } else if (ev::isNumber(args[0])) {
            token = static_cast<uint64_t>(ev::toDouble(args[0]));
        }
        if (token == 0) return ev::fromBool(false);
        return ev::fromBool(unwatchInternal(token));
    });

    // bro.apps.refresh()
    apps.def("refresh", 0, [](Value, std::span<const Value>) -> Value {
        auto catalog = activeCatalog();
        if (catalog) catalog->refresh();
        return ev::undefined();
    });

    // bro.apps.findByCategory(category) -> AppInfo[]
    apps.def("findByCategory", 1, [](Value, std::span<const Value> args) -> Value {
        if (args.empty() || !ev::isString(args[0])) return ev::makeArray(0);
        std::string cat = ev::toUtf8(args[0]);
        auto catalog = activeCatalog();
        if (!catalog) return ev::makeArray(0);

        auto results = catalog->find_by_category(cat);
        ev::Persistent arr(ev::makeArray(static_cast<uint32_t>(results.size())));
        for (uint32_t i = 0; i < results.size(); ++i) {
            ev::Persistent item(appInfoToJs(results[i]));
            ev::setElement(arr.get(), i, item.get());
        }
        return arr.get();
    });

    // bro.apps.findByMimeType(mimeType) -> AppInfo[]
    apps.def("findByMimeType", 1, [](Value, std::span<const Value> args) -> Value {
        if (args.empty() || !ev::isString(args[0])) return ev::makeArray(0);
        std::string mime = ev::toUtf8(args[0]);
        auto catalog = activeCatalog();
        if (!catalog) return ev::makeArray(0);

        auto results = catalog->find_by_mime_type(mime);
        ev::Persistent arr(ev::makeArray(static_cast<uint32_t>(results.size())));
        for (uint32_t i = 0; i < results.size(); ++i) {
            ev::Persistent item(appInfoToJs(results[i]));
            ev::setElement(arr.get(), i, item.get());
        }
        return arr.get();
    });

    // bro.apps.getCapabilities() -> { launcher, catalog }
    apps.def("getCapabilities", 0, [](Value, std::span<const Value>) -> Value {
        auto lcaps = query_launcher_capabilities();
        auto ccaps = query_catalog_capabilities();

        ObjectBuilder b;
        ObjectBuilder launcher;
        launcher.set("hasScopedIsolation", lcaps.has_scoped_isolation);
        launcher.set("hasPackagedAppLaunch", lcaps.has_packaged_app_launch);
        launcher.set("hasTerminalLaunch", lcaps.has_terminal_launch);
        launcher.set("hasJobObjectKill", lcaps.has_job_object_kill);
        launcher.set("hasSystemdCgroup", lcaps.has_systemd_cgroup);
        b.set("launcher", launcher.build());

        ObjectBuilder catalog;
        catalog.set("supportsCategories", ccaps.supports_categories);
        catalog.set("supportsKeywords", ccaps.supports_keywords);
        catalog.set("supportsMimeTypes", ccaps.supports_mime_types);
        catalog.set("supportsActions", ccaps.supports_actions);
        catalog.set("supportsPackagedApps", ccaps.supports_packaged_apps);
        b.set("catalog", catalog.build());

        return b.build();
    });
}

} // namespace broapps::api
