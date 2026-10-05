#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace broapps {

class ProcessHandle {
public:
    virtual ~ProcessHandle() = default;

    virtual uint64_t launch_id() const = 0;
    virtual int64_t pid() const = 0;
    virtual const std::string& scope_id() const = 0;

    virtual bool is_running() const = 0;
    virtual bool terminate() = 0;
    virtual bool kill() = 0;
    virtual bool wait_for_exit(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000)) = 0;
    virtual std::optional<int> exit_code() const = 0;
};

}  // namespace broapps
