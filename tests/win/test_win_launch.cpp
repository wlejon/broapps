#include "broapps/app_launcher.h"
#include "tests/test_common.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

int main(int argc, char* argv[]) {
#ifndef _WIN32
    (void)argc;
    (void)argv;
    TEST_SKIP("Windows-only test");
#else
    using namespace broapps;

    std::filesystem::path helper_path;
    if (argc > 1) {
        helper_path = argv[1];
    } else {
        // Fallback: look in same dir as current executable
        wchar_t mod_path[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, mod_path, MAX_PATH);
        helper_path = std::filesystem::path(mod_path).parent_path() / "broapps_test_helper.exe";
    }

    std::error_code ec;
    if (!std::filesystem::exists(helper_path, ec)) {
        TEST_SKIP("broapps_test_helper.exe not found at " + helper_path.string());
    }

    auto launcher = AppLauncher::create({true, true});
    TEST_CHECK(launcher != nullptr);

    // Test 1: Normal execution exit 0
    {
        LaunchScope scope;
        scope.arguments = {"--exit", "0"};

        auto handle = launcher->launch_executable(helper_path.string(), scope);
        TEST_CHECK(handle != nullptr);

        bool exited = handle->wait_for_exit(std::chrono::milliseconds(5000));
        TEST_CHECK(exited);
        TEST_CHECK_EQ(handle->exit_code().value_or(-1), 0);

        // Verify events in queue
        auto events = launcher->events().drain();
        TEST_CHECK(events.size() >= 2);
        TEST_CHECK(std::holds_alternative<AppStarted>(events[0]));
        TEST_CHECK(std::holds_alternative<AppExited>(events[1]));
        const auto& exited_ev = std::get<AppExited>(events[1]);
        TEST_CHECK_EQ(exited_ev.exit_code, 0);
    }

    // Test 2: Execution with non-zero exit code
    {
        LaunchScope scope;
        scope.arguments = {"--exit", "42"};

        auto handle = launcher->launch_executable(helper_path.string(), scope);
        TEST_CHECK(handle != nullptr);

        bool exited = handle->wait_for_exit(std::chrono::milliseconds(5000));
        TEST_CHECK(exited);
        TEST_CHECK_EQ(handle->exit_code().value_or(-1), 42);

        auto events = launcher->events().drain();
        TEST_CHECK(events.size() >= 2);
        const auto& exited_ev = std::get<AppExited>(events[1]);
        TEST_CHECK_EQ(exited_ev.exit_code, 42);
    }

    // Test 3: Process kill
    {
        LaunchScope scope;
        scope.arguments = {"--sleep", "10000"};

        auto handle = launcher->launch_executable(helper_path.string(), scope);
        TEST_CHECK(handle != nullptr);
        TEST_CHECK(handle->is_running());

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        bool killed = handle->kill();
        TEST_CHECK(killed);

        bool exited = handle->wait_for_exit(std::chrono::milliseconds(2000));
        TEST_CHECK(exited);
        TEST_CHECK(!handle->is_running());
    }

    std::cout << "test_win_launch passed!" << std::endl;
    return 0;
#endif
}
