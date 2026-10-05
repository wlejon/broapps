#include "job_object.h"

#ifdef _WIN32
namespace broapps::win_backend {

JobObject::JobObject() {
    h_ = CreateJobObjectW(nullptr, nullptr);
}

JobObject::JobObject(const std::wstring& name) {
    h_ = CreateJobObjectW(nullptr, name.empty() ? nullptr : name.c_str());
}

JobObject::~JobObject() {
    close();
}

void JobObject::close() {
    if (h_) {
        CloseHandle(h_);
        h_ = nullptr;
    }
}

JobObject::JobObject(JobObject&& other) noexcept
    : h_(other.h_) {
    other.h_ = nullptr;
}

JobObject& JobObject::operator=(JobObject&& other) noexcept {
    if (this != &other) {
        close();
        h_ = other.h_;
        other.h_ = nullptr;
    }
    return *this;
}

bool JobObject::set_kill_on_close(bool kill) {
    if (!h_) return false;

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {};
    if (kill) {
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK;
    }
    return SetInformationJobObject(h_, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli)) != 0;
}

bool JobObject::assign_process(HANDLE hProcess) {
    if (!h_ || !hProcess) return false;
    return AssignProcessToJobObject(h_, hProcess) != 0;
}

bool JobObject::terminate(UINT exit_code) {
    if (!h_) return false;
    return TerminateJobObject(h_, exit_code) != 0;
}

}  // namespace broapps::win_backend
#endif
