#pragma once

#include "broapps/process_handle.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>

namespace broapps {

class ProcessHandleBase : public ProcessHandle {
public:
    ProcessHandleBase(uint64_t launch_id, int64_t pid, std::string scope_id);
    ~ProcessHandleBase() override = default;

    uint64_t launch_id() const override { return launch_id_; }
    int64_t pid() const override { return pid_; }
    const std::string& scope_id() const override { return scope_id_; }

    bool is_running() const override;
    bool wait_for_exit(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000)) override;
    std::optional<int> exit_code() const override;

    void notify_started(int64_t pid);
    void notify_exited(int exit_code);

protected:
    uint64_t launch_id_;
    std::atomic<int64_t> pid_;
    std::string scope_id_;

    mutable std::mutex mutex_;
    std::condition_variable exit_cv_;
    std::atomic<bool> is_running_{true};
    std::optional<int> exit_code_;
};

}  // namespace broapps
