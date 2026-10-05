#include "broapps/icon_resolver.h"
#include "tests/test_common.h"

#include <filesystem>
#include <iostream>

int main() {
#ifndef __APPLE__
    TEST_SKIP("macOS-only test");
#else
    using namespace broapps;

    auto resolver = IconResolver::create();
    TEST_CHECK(resolver != nullptr);

    // Resolve an icon
    auto icon_path = resolver->resolve_icon("GenericApplicationIcon.icns");
    if (icon_path) {
        std::cout << "Resolved GenericApplicationIcon.icns -> " << *icon_path << std::endl;
        TEST_CHECK(std::filesystem::exists(*icon_path));
    }

    std::cout << "test_mac_icon passed!" << std::endl;
    return 0;
#endif
}
