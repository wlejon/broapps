#pragma once

#include "broapps/app_info.h"
#include "broapps/launch_scope.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace broapps::linux_backend {

// Parses desktop entry string content (from a .desktop file)
std::optional<AppInfo> parse_desktop_entry_string(
    std::string_view content,
    std::string_view desktop_file_path = {},
    std::string_view entry_id = {});

// Parses a .desktop file from the filesystem
std::optional<AppInfo> parse_desktop_entry_file(
    const std::string& filepath,
    std::string_view entry_id = {});

// Expands field codes (%f, %F, %u, %U, %i, %c, %k, %%) and unescapes Exec arguments.
// If action_id is specified in scope and exists in app.actions, that action's Exec is used.
std::vector<std::string> expand_exec(
    const AppInfo& app,
    const LaunchScope& scope);

// Utility for tokenizing raw Exec line into argv components with field code expansion
std::vector<std::string> tokenize_and_expand_exec(
    std::string_view exec_line,
    const AppInfo& app,
    const LaunchScope& scope);

}  // namespace broapps::linux_backend
