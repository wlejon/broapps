#include "broapps/app_launcher.h"
#include "tests/test_common.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

int main(int argc, char* argv[]) {
#ifndef __linux__
    (void)argc;
    (void)argv;
    TEST_SKIP("Linux-only test");
#else
    using namespace broapps;

    std::filesystem::path helper_path;
    if (argc > 1) {
        helper_path = argv[1];
    } else {
        helper_path = std::filesystem::current_path() / "tests" / "broapps_test_helper";
        if (!std::filesystem::exists(helper_path)) {
            helper_path = std::filesystem::current_path() / "broapps_test_helper";
        }
    }

    std::error_code ec;
    if (!std::filesystem::exists(helper_path, ec)) {
        TEST_SKIP("broapps_test_helper not found at " + helper_path.string());
    }

    auto launcher = AppLauncher::create({true, true});
    TEST_CHECK(launcher != nullptr);

    // Test 1: Exit 0
    {
        LaunchScope scope;
        scope.arguments = {"--exit", "0"};

        auto handle = launcher->launch_executable(helper_path.string(), scope);
        TEST_CHECK(handle != nullptr);

        bool exited = handle->wait_for_exit(std::chrono::milliseconds(5000));
        TEST_CHECK(exited);
        TEST_CHECK_EQ(handle->exit_code().value_or(-1), 0);

        auto events = launcher->events().drain();
        TEST_CHECK(events.size() >= 2);
        TEST_CHECK(std::holds_alternative<AppStarted>(events[0]));
        TEST_CHECK(std::holds_alternative<AppExited>(events[1]));
        const auto& exited_ev = std::get<AppExited>(events[1]);
        TEST_CHECK_EQ(exited_ev.exit_code, 0);
    }

    // Test 2: Exit 42
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

        bool exited = handle->wait_for_exit(std::chrono::milliseconds(3000));
        TEST_CHECK(exited);
        TEST_CHECK(!handle->is_running());
    }

    std::cout << "test_linux_launch passed!" << std::endl;
    return 0;
#endif
}
