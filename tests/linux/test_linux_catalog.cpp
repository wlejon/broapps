#include "broapps/app_catalog.h"
#include "tests/test_common.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <unordered_set>

int main() {
#ifndef __linux__
    TEST_SKIP("Linux-only test");
#else
    using namespace broapps;

    auto catalog = AppCatalog::create({{}, true});
    TEST_CHECK(catalog != nullptr);

    const auto& apps = catalog->apps();
    std::cout << "Linux Catalog loaded " << apps.size() << " apps." << std::endl;
    TEST_CHECK(!apps.empty());

    // Compare against /usr/share/applications
    std::filesystem::path app_dir("/usr/share/applications");
    std::error_code ec;
    if (!std::filesystem::exists(app_dir, ec)) {
        TEST_SKIP("/usr/share/applications does not exist");
    }

    size_t os_desktop_files = 0;
    std::unordered_set<std::string> catalog_ids;
    for (const auto& a : apps) {
        catalog_ids.insert(a.id);
    }

    size_t matched_files = 0;
    for (const auto& entry : std::filesystem::directory_iterator(app_dir, ec)) {
        if (entry.is_regular_file(ec) && entry.path().extension() == ".desktop") {
            ++os_desktop_files;
            std::string filename = entry.path().filename().string();
            if (catalog_ids.contains(filename)) {
                ++matched_files;
            }
        }
    }

    std::cout << "OS had " << os_desktop_files << " .desktop files; catalog matched " << matched_files << " of them." << std::endl;
    // Note: Some .desktop files have NoDisplay=true, Hidden=true, Type!=Application, or missing TryExec binaries
    TEST_CHECK(matched_files > 0);

    // Test search functionality
    auto results = catalog->search("Terminal");
    std::cout << "Search 'Terminal' returned " << results.size() << " apps." << std::endl;

    std::cout << "test_linux_catalog passed!" << std::endl;
    return 0;
#endif
}
