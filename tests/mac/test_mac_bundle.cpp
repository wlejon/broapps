#include "src/mac/plist_parser.h"
#include "tests/test_common.h"

#include <filesystem>
#include <iostream>

int main() {
#ifndef __APPLE__
    TEST_SKIP("macOS-only test");
#else
    using namespace broapps::mac_backend;

    std::string safari_path = "/Applications/Safari.app";
    if (!std::filesystem::exists(safari_path)) {
        safari_path = "/System/Applications/Safari.app";
    }

    if (!std::filesystem::exists(safari_path)) {
        TEST_SKIP("Safari.app not found on this macOS system");
    }

    auto app = parse_bundle_info_plist(safari_path);
    TEST_CHECK(app.has_value());
    TEST_CHECK_EQ(app->bundle_id, "com.apple.Safari");
    TEST_CHECK(!app->name.empty());
    TEST_CHECK(!app->executable_path.empty());
    TEST_CHECK(std::filesystem::exists(app->executable_path));

    std::cout << "test_mac_bundle passed!" << std::endl;
    return 0;
#endif
}
