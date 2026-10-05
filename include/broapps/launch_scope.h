#pragma once

#include <string>
#include <utility>
#include <vector>

namespace broapps {

struct LaunchScope {
    // Execution context
    std::string working_directory;
    std::vector<std::pair<std::string, std::string>> environment;
    std::vector<std::string> arguments;
    std::vector<std::string> files_to_open;

    // Execution mode
    bool terminal = false;
    bool detached = false;

    // Optional specific desktop action id (Linux desktop action)
    std::string action_id;

    // Isolation name hint (e.g. systemd user scope unit name or Windows Job Object name)
    std::string scope_name_hint;
};

}  // namespace broapps
