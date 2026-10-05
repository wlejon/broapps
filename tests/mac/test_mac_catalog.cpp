#include "broapps/app_catalog.h"
#include "tests/test_common.h"

#include <array>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <unordered_set>

#ifdef __APPLE__
static std::vector<std::string> get_mdfind_app_bundles() {
    std::vector<std::string> paths;
    FILE* pipe = popen("mdfind \"kMDItemContentType == 'com.apple.application-bundle'\" 2>/dev/null", "r");
    if (!pipe) return paths;

    std::array<char, 512> buffer;
    std::string current_line;
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        std::string chunk(buffer.data());
        for (char c : chunk) {
            if (c == '\r' || c == '\n') {
                if (!current_line.empty()) {
                    paths.push_back(current_line);
                    current_line.clear();
                }
            } else {
                current_line.push_back(c);
            }
        }
    }
    if (!current_line.empty()) paths.push_back(current_line);
    pclose(pipe);
    return paths;
}
#endif

int main() {
#ifndef __APPLE__
    TEST_SKIP("macOS-only test");
#else
    using namespace broapps;

    auto catalog = AppCatalog::create({{}, true});
    TEST_CHECK(catalog != nullptr);

    const auto& apps = catalog->apps();
    std::cout << "macOS Catalog loaded " << apps.size() << " apps." << std::endl;
    TEST_CHECK(!apps.empty());

    auto mdfind_apps = get_mdfind_app_bundles();
    std::cout << "mdfind reported " << mdfind_apps.size() << " application bundles." << std::endl;

    std::unordered_set<std::string> catalog_bundle_paths;
    for (const auto& a : apps) {
        if (!a.bundle_path.empty()) {
            catalog_bundle_paths.insert(a.bundle_path);
        }
    }

    size_t match_count = 0;
    for (const auto& p : mdfind_apps) {
        if (catalog_bundle_paths.contains(p)) {
            ++match_count;
        }
    }

    std::cout << "Matched " << match_count << " application bundles with mdfind." << std::endl;
    TEST_CHECK(match_count > 0);

    auto results = catalog->search("Safari");
    std::cout << "Search 'Safari' returned " << results.size() << " apps." << std::endl;

    std::cout << "test_mac_catalog passed!" << std::endl;
    return 0;
#endif
}
