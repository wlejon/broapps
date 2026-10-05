#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <variant>

namespace broapps {

struct AppStarted {
    uint64_t launch_id = 0;
    std::string app_id;
    int64_t pid = 0;
    std::string scope_id;
    std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now();
};

struct AppExited {
    uint64_t launch_id = 0;
    std::string app_id;
    int64_t pid = 0;
    int exit_code = 0;
    bool killed = false;
    std::string signal_name;
    std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now();
};

struct AppFailed {
    uint64_t launch_id = 0;
    std::string app_id;
    std::string error_message;
    std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now();
};

using LaunchEvent = std::variant<AppStarted, AppExited, AppFailed>;

}  // namespace broapps
