#include "process_handle_base.h"

namespace broapps {

ProcessHandleBase::ProcessHandleBase(uint64_t launch_id, int64_t pid, std::string scope_id)
    : launch_id_(launch_id), pid_(pid), scope_id_(std::move(scope_id)) {}

bool ProcessHandleBase::is_running() const {
    return is_running_.load();
}

bool ProcessHandleBase::wait_for_exit(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!is_running_.load()) return true;
    return exit_cv_.wait_for(lock, timeout, [this] { return !is_running_.load(); });
}

std::optional<int> ProcessHandleBase::exit_code() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return exit_code_;
}

void ProcessHandleBase::notify_started(int64_t pid) {
    pid_.store(pid);
}

void ProcessHandleBase::notify_exited(int exit_code) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        exit_code_ = exit_code;
        is_running_.store(false);
    }
    exit_cv_.notify_all();
}

}  // namespace broapps
