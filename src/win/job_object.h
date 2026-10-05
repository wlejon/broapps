#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>

namespace broapps::win_backend {

class JobObject {
public:
    JobObject();
    explicit JobObject(const std::wstring& name);
    ~JobObject();

    bool is_valid() const { return h_ != nullptr; }
    HANDLE native_handle() const { return h_; }

    bool set_kill_on_close(bool kill = true);
    bool assign_process(HANDLE hProcess);
    bool terminate(UINT exit_code = 1);
    void close();

    JobObject(const JobObject&) = delete;
    JobObject& operator=(const JobObject&) = delete;

    JobObject(JobObject&& other) noexcept;
    JobObject& operator=(JobObject&& other) noexcept;

private:
    HANDLE h_ = nullptr;
};

}  // namespace broapps::win_backend
#endif
