#include "broapps/icon_resolver.h"
#include "tests/test_common.h"

#include <filesystem>
#include <iostream>

int main() {
#ifndef __linux__
    TEST_SKIP("Linux-only test");
#else
    using namespace broapps;

    auto resolver = IconResolver::create();
    TEST_CHECK(resolver != nullptr);

    // Test resolving common icons
    static const char* icon_names[] = {
        "utilities-terminal",
        "system-search",
        "folder",
        "text-editor",
        "cmake",
        "btop"
    };

    bool found_at_least_one = false;
    for (const char* name : icon_names) {
        auto icon_path = resolver->resolve_icon(name, 48);
        if (icon_path) {
            std::cout << "Resolved icon '" << name << "' -> " << *icon_path << std::endl;
            TEST_CHECK(std::filesystem::exists(*icon_path));
            found_at_least_one = true;
        }
    }

    TEST_CHECK(found_at_least_one);

    std::cout << "test_linux_icon passed!" << std::endl;
    return 0;
#endif
}
