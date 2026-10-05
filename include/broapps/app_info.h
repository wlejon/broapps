#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace broapps {

struct DesktopAction {
    std::string id;
    std::string name;
    std::string exec;
    std::string icon;
};

struct AppInfo {
    // Canonical unique identifier:
    // Linux: desktop file id (e.g. "org.gnome.Terminal.desktop", "firefox.desktop")
    // Windows: AUMID or shortcut identifier (e.g. "Microsoft.WindowsTerminal_8wekyb3d8bbwe!App", "7-Zip.7zFM")
    // macOS: bundle identifier (e.g. "com.apple.Terminal", "org.mozilla.firefox")
    std::string id;

    // Human-readable names & descriptions
    std::string name;
    std::string generic_name;
    std::string comment;

    // Execution & icon
    std::string executable_path;
    std::string working_directory;
    std::string icon_name_or_path;

    // Classification & associations
    std::vector<std::string> categories;
    std::vector<std::string> keywords;
    std::vector<std::string> supported_mime_types;
    std::vector<std::string> supported_protocols;

    // Flags
    bool is_terminal = false;
    bool is_nodisplay = false;
    bool is_packaged = false;

    // Platform-specific metadata
    std::string desktop_file_path;      // Linux .desktop file path
    std::string exec_raw;               // Linux Exec= command line template
    std::string try_exec;               // Linux TryExec= binary
    std::vector<DesktopAction> actions; // Linux [Desktop Action <id>] entries

    std::string aumid;                  // Windows AppUserModelId
    std::string shortcut_path;          // Windows .lnk file path
    std::string arguments;              // Windows shortcut default arguments

    std::string bundle_id;              // macOS CFBundleIdentifier
    std::string bundle_path;            // macOS .app bundle directory path

    bool matches_query(std::string_view query) const;
    bool matches_category(std::string_view category) const;
    bool matches_mime_type(std::string_view mime_type) const;
};

}  // namespace broapps
