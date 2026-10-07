#include "win_launcher.h"
#include "com_init.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <filesystem>
#include <random>
#include <sstream>

namespace broapps::win_backend {

namespace {

std::wstring utf8_to_wide(std::string_view str) {
    if (str.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
    if (size <= 0) return {};
    std::wstring out(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), out.data(), size);
    return out;
}

std::string wide_to_utf8(std::wstring_view wstr) {
    if (wstr.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string out(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), out.data(), size, nullptr, nullptr);
    return out;
}

std::string quote_argument(std::string_view arg) {
    if (arg.empty()) return "\"\"";
    if (arg.find_first_of(" \t\n\v\"") == std::string_view::npos) {
        return std::string(arg);
    }

    std::string out;
    out.push_back('"');
    for (size_t i = 0; i < arg.size(); ++i) {
        size_t backslashes = 0;
        while (i < arg.size() && arg[i] == '\\') {
            ++backslashes;
            ++i;
        }

        if (i == arg.size()) {
            out.append(backslashes * 2, '\\');
            break;
        } else if (arg[i] == '"') {
            out.append(backslashes * 2 + 1, '\\');
            out.push_back('"');
        } else {
            out.append(backslashes, '\\');
            out.push_back(arg[i]);
        }
    }
    out.push_back('"');
    return out;
}

std::string generate_random_job_name(std::string_view hint) {
    static std::mt19937_64 rng(std::random_device{}());
    uint64_t val = rng();
    std::ostringstream ss;
    ss << "broapps-job-" << hint << "-" << std::hex << val;
    return ss.str();
}

BOOL CALLBACK TerminateAppEnumWindows(HWND hwnd, LPARAM lParam) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == static_cast<DWORD>(lParam)) {
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
    }
    return TRUE;
}

}  // namespace

WinProcessHandle::WinProcessHandle(
    uint64_t launch_id,
    int64_t pid,
    std::string scope_id,
    HANDLE hProcess,
    HANDLE hThread,
    JobObject job)
    : ProcessHandleBase(launch_id, pid, std::move(scope_id)),
      hProcess_(hProcess),
      hThread_(hThread),
      job_(std::move(job)) {}

WinProcessHandle::~WinProcessHandle() {
    if (hThread_) {
        CloseHandle(hThread_);
        hThread_ = nullptr;
    }
    if (hProcess_) {
        CloseHandle(hProcess_);
        hProcess_ = nullptr;
    }
}

bool WinProcessHandle::terminate() {
    if (!is_running_.load()) return false;
    int64_t p = pid_.load();
    if (p > 0) {
        EnumWindows(TerminateAppEnumWindows, static_cast<LPARAM>(p));
    }
    return true;
}

bool WinProcessHandle::kill() {
    if (!is_running_.load()) return false;
    bool success = false;
    if (job_.is_valid()) {
        success = job_.terminate(1);
    }
    if (hProcess_) {
        if (TerminateProcess(hProcess_, 1)) {
            success = true;
        }
    }
    return success;
}

WinLauncher::WinLauncher(LauncherConfig config)
    : config_(config) {}

WinLauncher::~WinLauncher() = default;

std::shared_ptr<ProcessHandle> WinLauncher::launch(
    const AppInfo& app,
    const LaunchScope& scope) {

    uint64_t lid = next_launch_id_++;

    if (app.is_packaged || (!app.aumid.empty() && app.executable_path.empty())) {
        return launch_packaged(app, scope, lid);
    }

    std::string exec = app.executable_path;
    if (exec.empty() && !app.shortcut_path.empty()) {
        exec = app.shortcut_path;
    }

    return launch_win32(app.id, exec, app.arguments, scope, lid);
}

std::shared_ptr<ProcessHandle> WinLauncher::launch_executable(
    const std::string& exec_path,
    const LaunchScope& scope) {

    uint64_t lid = next_launch_id_++;
    std::string app_id;
    try {
        app_id = std::filesystem::path(exec_path).filename().string();
    } catch (...) {
        app_id = exec_path;
    }

    return launch_win32(app_id, exec_path, "", scope, lid);
}

std::shared_ptr<ProcessHandle> WinLauncher::launch_packaged(
    const AppInfo& app,
    const LaunchScope& scope,
    uint64_t lid) {

    std::string aumid = app.aumid.empty() ? app.id : app.aumid;
    std::wstring aumid_w = utf8_to_wide(aumid);

    std::string args_combined;
    for (const auto& a : scope.arguments) {
        if (!args_combined.empty()) args_combined.push_back(' ');
        args_combined.append(quote_argument(a));
    }
    for (const auto& f : scope.files_to_open) {
        if (!args_combined.empty()) args_combined.push_back(' ');
        args_combined.append(quote_argument(f));
    }
    std::wstring args_w = utf8_to_wide(args_combined);

    DWORD pid = 0;
    bool activated = false;
    AllowSetForegroundWindow(ASFW_ANY);

    // 1. Try IApplicationActivationManager in-process (twinui.appcore.dll).
    // Note: In a standalone shell session where explorer.exe is not running,
    // CLSCTX_LOCAL_SERVER attempts to activate an out-of-process surrogate via dllhost.exe
    // configured with RunAs: Interactive User, which deadlocks waiting for the shell broker.
    // Using CLSCTX_INPROC_SERVER loads twinui.appcore.dll directly without surrogate processes.
    {
        ComScope com;
        IApplicationActivationManager* aam = nullptr;
        HRESULT hr = CoCreateInstance(
            CLSID_ApplicationActivationManager,
            nullptr,
            CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER,
            IID_IApplicationActivationManager,
            reinterpret_cast<void**>(&aam));

        if (SUCCEEDED(hr) && aam) {
            hr = aam->ActivateApplication(
                aumid_w.c_str(),
                args_w.empty() ? nullptr : args_w.c_str(),
                AO_NONE,
                &pid);
            aam->Release();
            if (SUCCEEDED(hr)) {
                activated = true;
            }
        }
    }

    // 2. Fallback: ShellExecuteExW with shell:AppsFolder\<AUMID> or protocol schemes
    if (!activated) {
        std::wstring target = L"shell:AppsFolder\\" + aumid_w;
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOCLOSEPROCESS;
        sei.lpVerb = L"open";
        sei.lpFile = target.c_str();
        sei.lpParameters = args_w.empty() ? nullptr : args_w.c_str();
        sei.nShow = SW_SHOWNORMAL;
        if (ShellExecuteExW(&sei)) {
            activated = true;
            if (sei.hProcess) {
                pid = GetProcessId(sei.hProcess);
                CloseHandle(sei.hProcess);
            }
        }
    }

    if (pid > 0) {
        AllowSetForegroundWindow(pid);
    }

    if (!activated) {
        std::string err = "Failed to activate packaged app " + aumid;
        events_.push(AppFailed{lid, app.id, err, std::chrono::system_clock::now()});
        auto h = std::make_shared<WinProcessHandle>(lid, 0, "", nullptr, nullptr, JobObject{});
        h->notify_exited(1);
        return h;
    }

    HANDLE hProcess = nullptr;
    if (pid > 0) {
        hProcess = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!hProcess) {
            hProcess = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        }
    }

    // Note: Do not assign packaged / UWP apps to a JobObject with kill_on_close.
    // Windows AppModel natively manages AppContainers, and assigning them to a custom
    // JobObject can fail with access denied or kill the app prematurely upon handle close.
    auto handle = std::make_shared<WinProcessHandle>(
        lid, static_cast<int64_t>(pid), "", hProcess, nullptr, JobObject{});

    events_.push(AppStarted{
        lid,
        app.id,
        static_cast<int64_t>(pid),
        "",
        std::chrono::system_clock::now()
    });

    if (hProcess) {
        std::thread([this, handle, lid, app_id = app.id, hProcess, pid]() {
            WaitForSingleObject(hProcess, INFINITE);
            DWORD exit_code = 0;
            GetExitCodeProcess(hProcess, &exit_code);

            events_.push(AppExited{
                lid,
                app_id,
                static_cast<int64_t>(pid),
                static_cast<int>(exit_code),
                false,
                "",
                std::chrono::system_clock::now()
            });

            handle->notify_exited(static_cast<int>(exit_code));
        }).detach();
    }

    return handle;
}

std::shared_ptr<ProcessHandle> WinLauncher::launch_win32(
    const std::string& app_id,
    const std::string& exec_path,
    const std::string& existing_args,
    const LaunchScope& scope,
    uint64_t lid) {

    if (exec_path.empty()) {
        events_.push(AppFailed{lid, app_id, "No executable path provided", std::chrono::system_clock::now()});
        auto h = std::make_shared<WinProcessHandle>(lid, 0, "", nullptr, nullptr, JobObject{});
        h->notify_exited(1);
        return h;
    }

    std::string cmdline = quote_argument(exec_path);
    if (!existing_args.empty()) {
        cmdline.push_back(' ');
        cmdline.append(existing_args);
    }
    for (const auto& a : scope.arguments) {
        cmdline.push_back(' ');
        cmdline.append(quote_argument(a));
    }
    for (const auto& f : scope.files_to_open) {
        cmdline.push_back(' ');
        cmdline.append(quote_argument(f));
    }

    std::wstring cmdline_w = utf8_to_wide(cmdline);
    std::wstring workdir_w;
    if (!scope.working_directory.empty()) {
        workdir_w = utf8_to_wide(scope.working_directory);
    }

    std::vector<wchar_t> env_block;
    if (!scope.environment.empty()) {
        // Build environment block: inherit current environment and merge scope.environment
        LPWCH curr_env = GetEnvironmentStringsW();
        if (curr_env) {
            LPWCH p = curr_env;
            while (*p) {
                size_t len = wcslen(p);
                std::wstring_view entry(p, len);
                auto eq = entry.find(L'=');
                if (eq != std::wstring_view::npos && eq > 0) {
                    std::string key = wide_to_utf8(entry.substr(0, eq));
                    bool overridden = false;
                    for (const auto& [ek, ev] : scope.environment) {
                        if (_stricmp(key.c_str(), ek.c_str()) == 0) {
                            overridden = true;
                            break;
                        }
                    }
                    if (!overridden) {
                        env_block.insert(env_block.end(), p, p + len + 1);
                    }
                }
                p += len + 1;
            }
            FreeEnvironmentStringsW(curr_env);
        }
        for (const auto& [k, v] : scope.environment) {
            std::wstring item = utf8_to_wide(k) + L"=" + utf8_to_wide(v);
            env_block.insert(env_block.end(), item.begin(), item.end());
            env_block.push_back(L'\0');
        }
        env_block.push_back(L'\0');
    }

    DWORD creation_flags = CREATE_UNICODE_ENVIRONMENT | CREATE_NEW_PROCESS_GROUP;
    if (config_.use_job_objects) {
        creation_flags |= CREATE_SUSPENDED;
    }
    if (scope.terminal) {
        creation_flags |= CREATE_NEW_CONSOLE;
    } else if (scope.detached) {
        creation_flags |= DETACHED_PROCESS;
    }

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};

    AllowSetForegroundWindow(ASFW_ANY);

    BOOL ok = CreateProcessW(
        nullptr,
        cmdline_w.data(),
        nullptr,
        nullptr,
        FALSE,
        creation_flags,
        env_block.empty() ? nullptr : env_block.data(),
        workdir_w.empty() ? nullptr : workdir_w.c_str(),
        &si,
        &pi);

    if (!ok) {
        DWORD err = GetLastError();
        std::string err_msg = "CreateProcessW failed with error " + std::to_string(err);
        events_.push(AppFailed{lid, app_id, err_msg, std::chrono::system_clock::now()});
        auto h = std::make_shared<WinProcessHandle>(lid, 0, "", nullptr, nullptr, JobObject{});
        h->notify_exited(static_cast<int>(err));
        return h;
    }

    AllowSetForegroundWindow(pi.dwProcessId);

    std::string job_name = generate_random_job_name(app_id);
    JobObject job;
    if (config_.use_job_objects) {
        job = JobObject(utf8_to_wide(job_name));
        if (job.is_valid()) {
            job.set_kill_on_close(true);
            job.assign_process(pi.hProcess);
        }
        ResumeThread(pi.hThread);
    }

    auto handle = std::make_shared<WinProcessHandle>(
        lid, static_cast<int64_t>(pi.dwProcessId), job_name, pi.hProcess, pi.hThread, std::move(job));

    events_.push(AppStarted{
        lid,
        app_id,
        static_cast<int64_t>(pi.dwProcessId),
        job_name,
        std::chrono::system_clock::now()
    });

    HANDLE hProc = pi.hProcess;
    DWORD pid = pi.dwProcessId;

    std::thread([this, handle, lid, app_id, hProc, pid]() {
        WaitForSingleObject(hProc, INFINITE);
        DWORD exit_code = 0;
        GetExitCodeProcess(hProc, &exit_code);

        events_.push(AppExited{
            lid,
            app_id,
            static_cast<int64_t>(pid),
            static_cast<int>(exit_code),
            false,
            "",
            std::chrono::system_clock::now()
        });

        handle->notify_exited(static_cast<int>(exit_code));
    }).detach();

    return handle;
}

}  // namespace broapps::win_backend

namespace broapps {

std::unique_ptr<AppLauncher> AppLauncher::create(const LauncherConfig& config) {
    return std::make_unique<win_backend::WinLauncher>(config);
}

}  // namespace broapps
#endif
