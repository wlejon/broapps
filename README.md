# broapps

[![CI](https://github.com/wlejon/broapps/actions/workflows/ci.yml/badge.svg)](https://github.com/wlejon/broapps/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

Application catalog and scoped process execution substrate for a cross-platform desktop
environment: discovering installed applications, launching processes in isolated scopes,
querying and persisting file type (MIME) associations, tracking recent documents, and
resolving desktop icon assets. A standalone C++20 library with native backends for Linux,
Windows, and macOS.

broapps sits in the desktop-environment layer of the
[bro ecosystem](https://github.com/wlejon/bro/blob/main/docs/ecosystem.md). It builds directly
on its sibling **[brovfs](https://github.com/wlejon/brovfs)** for file type querying
(`brovfs::MimeDatabase`) and directory change monitoring (`brovfs::DirectoryWatcher`).
The [bro runtime](https://github.com/wlejon/bro) mounts its JavaScript binding
(`broapps_api` in `src/api/`) onto `bro.apps` under the `BRO_WITH_APPS` build gate, exposing
application search, process launching, and default app configuration to apps running on
bronze.

## Architecture & Model

Applications are discovered and launched through platform-native facilities. Background worker
threads push value snapshots and lifecycle events into a thread-safe `MessageQueue`
(`event_queue.h`), which the host drains on its own loop:

- **No host callback re-entrancy:** Backend threads do not execute host code; an optional
  wake hook posts to the host's event loop.
- **Snapshot queries:** Catalog queries return immutable snapshots of application metadata.
- **Scoped execution:** Launching produces a `ProcessHandle` and isolates child processes
  within cgroups (`systemd-run`) or Win32 Job Objects.
- **Event-driven monitoring:** Process start, exit codes, and signals stream to the host as
  `AppStarted`, `AppExited`, or `AppFailed` events.

```cpp
#include <broapps/broapps.h>

// 1. Discover applications
auto catalog = broapps::AppCatalog::create();
auto terminal_apps = catalog->search("Terminal");

// 2. Launch with scoped isolation
auto launcher = broapps::AppLauncher::create();
launcher->events().set_wake([] { /* post wake to host event loop */ });

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
  broapps.h          Umbrella header
  app_info.h         AppInfo, DesktopAction
  launch_scope.h     LaunchScope (working directory, env, arguments, files, terminal, detached)
  launch_event.h     AppStarted, AppExited, AppFailed (std::variant<...>)
  process_handle.h   ProcessHandle (is_running, terminate, kill, wait_for_exit, exit_code)
  event_queue.h      MessageQueue<T> (thread-safe MPSC queue with wake hook)
  app_catalog.h      AppCatalog (querying, fuzzy search with scoring, categories, MIME types)
  app_launcher.h     AppLauncher (scoped launching of AppInfo or arbitrary executables)
  mime_service.h     MimeService (default app querying, set_default_app_for_mime, open-with)
  icon_resolver.h    IconResolver (path resolution for desktop icons across themes)
  recent_service.h   RecentService (querying, registering, and clearing recent documents)
  catalog_watcher.h  CatalogWatcher (detects application install/remove/update via brovfs)
  capabilities.h     LauncherCapabilities, CatalogCapabilities (honest platform reporting)
  api.h              Bronze JavaScript binding entry point (broapps_api)
```

## Platform Backends

| Capability | Linux | Windows | macOS |
|---|---|---|---|
| **App Catalog** | XDG Application directories (`$XDG_DATA_DIRS`, `$XDG_DATA_HOME`), Desktop Entry Spec v1.5 (`.desktop` parser, `%f/%F/%u/%U/%i/%c/%k` field code expansions, sub-actions, `TryExec` checks) | Start Menu shortcuts (`.lnk` via COM `IShellLinkW` / `IPersistFile`), Packaged Store apps via `shell:AppsFolder` enumeration (`IShellItem2`, `PKEY_AppUserModel_ID`) | `/Applications`, `/System/Applications`, `~/Applications`, bundle scanner reading `Info.plist` via CoreFoundation / Foundation |
| **Scoped Launching** | `systemd-run --user --scope --unit=app-<name>-<uuid>` (cgroup user scopes); detached `setsid` / `fork` / `execv` fallback | Win32 Job Object isolation (`CreateJobObjectW`, `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`, suspended start, `ResumeThread`); `IApplicationActivationManager` for packaged AUMIDs | Executable `fork`/`exec` with process group isolation, or `open -b <bundle_id> --args` |
| **Process Monitoring** | Background worker thread waiting on `waitpid` (reports exit codes and signal names) | Background worker thread waiting on `WaitForSingleObject` and `GetExitCodeProcess` | Background worker thread waiting on `waitpid` |
| **MIME Associations** | Freedesktop `mimeapps.list` spec hierarchy (`~/.config/mimeapps.list`, `/etc/xdg/mimeapps.list`, `mimeinfo.cache`), atomic `mimeapps.list` user default persistence | Windows Registry (`UserChoice`, `OpenWithProgids`, `OpenWithList` under `HKCU` and `HKCR`), shell command parsing | macOS LaunchServices API (`LSSetDefaultRoleHandlerForContentType`, `LSCopyDefaultRoleHandlerForContentType`, `UTType`) |
| **Recent Documents** | Freedesktop Desktop Bookmark Spec (`recently-used.xbel` XML parser & atomic writer) | Win32 Shell Recent items (`SHGetKnownFolderPath(FOLDERID_Recent)` `.lnk` parsing and `SHAddToRecentDocs`) | Property list (`.plist`) backed recent items storage |
| **Icon Resolution** | Freedesktop Icon Theme Spec (theme hierarchies, `/usr/share/pixmaps`, size fallbacks) | Win32 icon extraction (`ExtractIconExW`, `PrivateExtractIconsW`) and index parsing | macOS `CoreTypes` bundle resources & `.icns` file resolution |
| **Catalog Watcher** | `brovfs::DirectoryWatcher` (inotify) over XDG application directories | `brovfs::DirectoryWatcher` (ReadDirectoryChangesExW) over Start Menu directories | `brovfs::DirectoryWatcher` (FSEvents) over application bundle directories |

## Building & Dependencies

broapps requires CMake 3.24+ and a C++20 compiler.

### Dependency Resolution (brovfs)

broapps depends on **[brovfs](https://github.com/wlejon/brovfs)**. There are no submodules:
brovfs (and bronze, for the JavaScript binding) is a `bro_dependency()` pin in
`CMakeLists.txt`, resolved through `cmake/bro_deps.cmake` in this order:
1. **Existing CMake target:** Uses `brovfs` if already provided by a parent superbuild.
2. **Working tree:** `../brovfs` beside the top-level project (or `-DFETCHCONTENT_SOURCE_DIR_BROVFS=<path>`).
3. **Pinned commit:** fetched from GitHub at configure, so a plain `git clone` builds.

### Standalone Build

```bash
# Linux (GCC / Clang + Ninja)
sudo apt install ninja-build shared-mime-info hicolor-icon-theme desktop-file-utils xdg-utils
cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
ctest --test-dir build-release --output-on-failure

# Windows (MSVC, Visual Studio 2022 or Ninja)
cmake -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure

# macOS (Apple Clang + Ninja)
cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
ctest --test-dir build-release --output-on-failure
```

### Consuming broapps

Downstream projects link against `broapps::broapps`:

```cmake
add_subdirectory(path/to/broapps)
target_link_libraries(your_target PRIVATE broapps::broapps)
```

The standalone Bronze JavaScript binding (`BROAPPS_ENABLE_API`, on when broapps is the
top-level project) compiles `broapps_api` for the [bronze](https://github.com/wlejon/bronze)
runtime. bronze (with brass) resolves like brovfs: `../bronze` beside the top-level project,
else the pinned commit. Set `-DBROAPPS_ENABLE_API=OFF` to disable the JavaScript binding.

## Tests & Verification

The test suite runs real ctests without mocking library code. Tests exit with status `77`
(skip) when an environment dependency is genuinely absent, printing an explanatory reason.
Tests leave zero lasting changes or orphaned processes.

### Test Matrix

| Test | Platform | Target & Verification Method |
|---|---|---|
| `test_event_queue` | All | Multi-producer concurrent pushes, FIFO ordering, wake hooks, `wait_for` timeouts |
| `test_search` | All | Exact ID/name matches, keyword scoring, categories, and MIME wildcard filters |
| `test_mime_service` | All | Type queries forward to brovfs `MimeDatabase`; association resolution without infinite recursion |
| `test_catalog_watcher` | All | Synthetic entries created, modified, and unlinked in temp catalog dirs trigger incremental events |
| `test_linux_catalog` | Linux | Differential validation against `/usr/share/applications` |
| `test_desktop_parse` | Linux | Freedesktop spec v1.5 parsing, field code expansion (`%F`, `%U`, `%i`, `%c`, `%k`), sub-actions |
| `test_linux_launch` | Linux | Real process execution in scoped `systemd-run` user scope & fallback paths, signal termination |
| `test_linux_mime` | Linux | Freedesktop `mimeapps.list` association hierarchy, default app resolution, and atomic user default persistence |
| `test_xbel` / `test_linux_recent` | Linux | Freedesktop XBEL XML parsing and atomic recent document management |
| `test_linux_icon` | Linux | Freedesktop icon theme path and dimension resolution |
| `test_win_catalog` | Windows | Differential validation against PowerShell `Get-StartApps` |
| `test_win_lnk` | Windows | Start Menu `.lnk` parsing via COM `IShellLinkW` |
| `test_win_launch` | Windows | Real process lifecycle under Win32 Job Objects (clean exits, exit codes, process group termination) |
| `test_win_mime` | Windows | Registry association querying under `HKCU` and `HKCR` |
| `test_win_recent` | Windows | Win32 Shell Recent items parsing via `FOLDERID_Recent` |
| `test_mac_catalog` | macOS | Differential comparison against `mdfind` application bundles |
| `test_mac_bundle` | macOS | Real `/Applications` bundle `Info.plist` parsing |
| `test_mac_launch` | macOS | Real process execution, signal termination, and exit code reporting |
| `test_mac_mime` | macOS | LaunchServices MIME and UTI handler candidate queries |
| `test_mac_recent` | macOS | macOS recent items property list serialization |
| `test_mac_icon` | macOS | CoreTypes `.icns` asset resolution |

### Real Process Lifecycle & MIME Persistence Testing

- **Process Lifecycle Verification:** `test_linux_launch`, `test_win_launch`, and
  `test_mac_launch` launch real binaries (such as helper test binaries or system utilities)
  under strict execution scopes. The tests verify environment inheritance, working directory
  setup, standard output handling, clean exit codes (`0`, `42`), and force-kill signal
  handling with Job Objects or cgroups.
- **MIME Default App Persistence:** `test_linux_mime` exercises `set_default_app_for_mime()`
  by parsing existing `mimeapps.list` files, writing user overrides to an isolated scratch
  config directory (`$XDG_CONFIG_HOME`), and verifying that subsequent queries return the new
  default without corrupting existing associations.