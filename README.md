# broapps

Application catalog and scoped process execution substrate for a cross-platform desktop environment built on the bro runtime. A standalone C++20 library: no dependency on bro, bronze or sibling libraries, no JS bindings, its own CMake and ctest.

## Model

Applications are discovered and launched through platform-native mechanisms. Backends push value snapshots into the service's `MessageQueue` (`event_queue.h`); the host drains it on its own thread. No callback runs host code except the queue's optional wake hook. Queries return value snapshots; commands return a `ProcessHandle`, and their lifecycle shows up as events. There are no mocks or test setters in the public API.

```cpp
#include <broapps/broapps.h>

// 1. Discover applications
auto catalog = broapps::AppCatalog::create();
auto terminal_apps = catalog->search("Terminal");

// 2. Launch with scoped isolation
auto launcher = broapps::AppLauncher::create();
launcher->events().set_wake([] { /* post to host event loop */ });

broapps::LaunchScope scope;
scope.arguments = {"--profile", "default"};
auto handle = launcher->launch(terminal_apps.front(), scope);

// On host thread:
for (const auto& ev : launcher->events().drain()) {
    if (auto* started = std::get_if<broapps::AppStarted>(&ev)) {
        std::cout << "App started: PID=" << started->pid << " Scope=" << started->scope_id << "\n";
    } else if (auto* exited = std::get_if<broapps::AppExited>(&ev)) {
        std::cout << "App exited with code: " << exited->exit_code << "\n";
    }
}
```

```
include/broapps/
  app_info.h         AppInfo, DesktopAction
  launch_scope.h     LaunchScope (working directory, env, arguments, files, terminal, detached)
  launch_event.h     AppStarted, AppExited, AppFailed (std::variant<...>)
  process_handle.h   ProcessHandle (is_running, terminate, kill, wait_for_exit, exit_code)
  event_queue.h      MessageQueue<T> (thread-safe MPSC queue with wake hook)
  app_catalog.h      AppCatalog (querying, search with scoring, categories, MIME types, refresh)
  app_launcher.h     AppLauncher (scoped launch of AppInfo or arbitrary executables)
  mime_service.h     MimeService (default apps, open-with candidates, MIME <-> extension)
  icon_resolver.h    IconResolver (path resolution for desktop icons across themes)
  recent_service.h   RecentService (querying, registering, and clearing recent documents)
  catalog_watcher.h  CatalogWatcher (file system change notification on installed apps)
  capabilities.h     LauncherCapabilities, CatalogCapabilities (honest platform reporting)
  broapps.h          Umbrella include
```

## Backends

| Capability | Linux | Windows | macOS |
|---|---|---|---|
| **App Catalog** | XDG Application directories (`$XDG_DATA_HOME`, `$XDG_DATA_DIRS`), Freedesktop Desktop Entry Specification v1.5 (`.desktop` parser, `%f/%F/%u/%U/%i/%c/%k` field code expansions, sub-actions, deduplication, TryExec checks) | Start Menu shortcuts (`.lnk` via COM `IShellLinkW` / `IPersistFile`), Packaged Apps & modern Store apps via `shell:AppsFolder` enumeration (`IShellItem2` PKEY_AppUserModel_ID) | `/Applications`, `/System/Applications`, `~/Applications`, `Info.plist` bundle scanner via CoreFoundation / Foundation, `.icns` resource resolution |
| **Scoped Launching** | `systemd-run --user --scope --unit=app-<name>-<uuid>` (isolated cgroup user scopes); detached `setsid` / `fork` / `execv` fallback | Win32 Job Object isolation (`CreateJobObjectW`, `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`, suspended start, `ResumeThread`); `IApplicationActivationManager` for packaged AUMIDs | Direct executable `fork`/`exec` with process group isolation, or `open -b <bundle_id> --args` |
| **Process Monitoring** | Background worker thread waiting on `waitpid`, pushes `AppStarted` and `AppExited` (exit code, signal name) | Background worker thread waiting on `WaitForSingleObject` and `GetExitCodeProcess` | Background worker thread waiting on `waitpid` |
| **MIME Associations** | Freedesktop `mimeapps.list` spec hierarchy (`~/.config/mimeapps.list`, `/etc/xdg/mimeapps.list`, `/usr/share/applications/mimeinfo.cache`) | Windows Registry (`UserChoice` / `OpenWithProgids` / `OpenWithList` under `HKCU` and `HKCR`), shell command parsing | macOS LaunchServices API (`LSCopyDefaultRoleHandlerForContentType`, `LSCopyAllRoleHandlersForContentType`, `UTType`) |
| **Recent Documents** | Freedesktop Desktop Bookmark Spec (`recently-used.xbel` XML reader & atomic writer) | Win32 Shell Recent items (`SHGetKnownFolderPath(FOLDERID_Recent)` `.lnk` parsing and `SHAddToRecentDocs`) | Property list backed recent items storage |
| **Icon Resolution** | Freedesktop Icon Theme Spec (theme hierarchies, `/usr/share/pixmaps`, size fallback) | Win32 icon path and index extraction | macOS CoreTypes bundle resources & `.icns` resolution |
| **Catalog Watcher** | Linux `inotify` watching XDG data directories for changes | Win32 `ReadDirectoryChangesW` watching Start Menu hierarchies | macOS `FSEventStream` watching `/Applications` and `~/Applications` |

## Building

Windows (MSVC, Visual Studio generator or Ninja):

```bash
cmake -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Linux (GCC 12+, Arch / Debian):

```bash
cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
ctest --test-dir build-release --output-on-failure
```

macOS (Apple Clang, Command Line Tools or Xcode):

```bash
export PATH=/opt/homebrew/bin:$PATH
cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
ctest --test-dir build-release --output-on-failure
```

## Tests

Real ctests: no `assert()`, and failures count in every configuration. Exit 77 is a skip, used only when a tool or platform feature is absent, printing the reason. Tests are non-destructive and leave zero lasting changes or lingering processes.

| Test | Platform | Oracle / Target |
|---|---|---|
| `test_event_queue` | All | Multi-producer concurrent pushes, FIFO ordering, wake hooks, `wait_for` timeouts |
| `test_search` | All | Exact ID / name matches, keyword scoring, generic name, category & MIME wildcard filtering |
| `test_mime_table` | All | Static extension-to-MIME and MIME-to-extension mappings |
| `test_win_lnk` | Windows | Start Menu `.lnk` parsing via real COM `IShellLinkW` |
| `test_win_catalog` | Windows | Differential comparison against PowerShell `Get-StartApps` |
| `test_win_launch` | Windows | Scoped Job Object lifecycle (started -> exited code 0, 42, kill) on real processes |
| `test_win_mime` | Windows | Windows registry association querying (`.txt`, `.html`, ProgIDs) |
| `test_win_recent` | Windows | Win32 Shell Recent items parsing |
| `test_win_watcher` | Windows | Win32 `ReadDirectoryChangesW` watcher start, lifecycle, and teardown |
| `test_desktop_parse` | Linux | Freedesktop spec v1.5 parsing, field code expansion (`%F`, `%U`, `%i`, `%c`, `%k`), sub-actions |
| `test_linux_catalog` | Linux | Differential comparison against `/usr/share/applications` |
| `test_linux_launch` | Linux | Scoped `systemd-run` user scope & fallback lifecycle, kill signal verification |
| `test_xbel` | Linux | Freedesktop XBEL XML parsing and serialization |
| `test_linux_mime` | Linux | Freedesktop `mimeapps.list` association hierarchy and default app resolution |
| `test_linux_recent` | Linux | Linux recent files management (`recently-used.xbel`) |
| `test_linux_icon` | Linux | Freedesktop icon theme path resolution |
| `test_linux_watcher` | Linux | Linux `inotify` watcher start, lifecycle, and teardown |
| `test_mac_bundle` | macOS | Real `/Applications` bundle `Info.plist` parsing |
| `test_mac_catalog` | macOS | Differential comparison against `mdfind` application bundles |
| `test_mac_launch` | macOS | Scoped launch lifecycle and signal termination verification |
| `test_mac_mime` | macOS | macOS LaunchServices MIME and UTI handler candidate queries |
| `test_mac_recent` | macOS | macOS recent documents item serialization and management |
| `test_mac_icon` | macOS | macOS CoreTypes `.icns` path resolution |
| `test_mac_watcher` | macOS | macOS `FSEventStream` watcher start, lifecycle, and teardown |
