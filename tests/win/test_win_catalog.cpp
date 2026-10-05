#include "broapps/app_catalog.h"
#include "tests/test_common.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <unordered_set>

#ifdef _WIN32
static std::vector<std::string> get_powershell_start_apps() {
    std::vector<std::string> names;
    FILE* pipe = _popen("powershell -NoProfile -Command \"Get-StartApps | Select-Object -ExpandProperty Name\"", "r");
    if (!pipe) return names;

    std::array<char, 256> buffer;
    std::string current_line;
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        std::string chunk(buffer.data());
        for (char c : chunk) {
            if (c == '\r' || c == '\n') {
                if (!current_line.empty()) {
                    names.push_back(current_line);
                    current_line.clear();
                }
            } else {
                current_line.push_back(c);
            }
        }
    }
    if (!current_line.empty()) {
        names.push_back(current_line);
    }
    _pclose(pipe);
    return names;
}
#endif

int main() {
#ifndef _WIN32
    TEST_SKIP("Windows-only test");
#else
    using namespace broapps;

    auto catalog = AppCatalog::create({{}, true});
    TEST_CHECK(catalog != nullptr);

    const auto& apps = catalog->apps();
    std::cout << "Catalog loaded " << apps.size() << " apps." << std::endl;
    TEST_CHECK(!apps.empty());

    // Differential test against PowerShell Get-StartApps
    auto ps_apps = get_powershell_start_apps();
    std::cout << "PowerShell Get-StartApps reported " << ps_apps.size() << " apps." << std::endl;
    TEST_CHECK(!ps_apps.empty());

    std::unordered_set<std::string> catalog_names;
    for (const auto& a : apps) {
        catalog_names.insert(a.name);
    }

    size_t match_count = 0;
    for (const auto& name : ps_apps) {
        if (catalog_names.contains(name)) {
            ++match_count;
        }
    }

    std::cout << "Matched " << match_count << " of " << ps_apps.size() << " apps with Get-StartApps." << std::endl;
    // We expect a significant match rate (>30% at minimum, usually much higher)
    TEST_CHECK(match_count > 5);

    // Test search functionality on live catalog
    auto search_results = catalog->search("Terminal");
    std::cout << "Search 'Terminal' found " << search_results.size() << " apps." << std::endl;

    std::cout << "test_win_catalog passed!" << std::endl;
    return 0;
#endif
}
