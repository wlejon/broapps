#include "broapps/app_catalog.h"
#include "src/win/lnk_parser.h"
#include "src/win/com_init.h"
#include "tests/test_common.h"

#include <iostream>

int main() {
#ifndef _WIN32
    TEST_SKIP("Windows-only test");
#else
    using namespace broapps::win_backend;

    ComScope com;
    TEST_CHECK(com.succeeded());

    auto shortcuts = scan_start_menu_shortcuts({}, true);
    std::cout << "Discovered " << shortcuts.size() << " Start Menu shortcuts." << std::endl;
    TEST_CHECK(!shortcuts.empty());

    bool found_valid_exe = false;
    for (const auto& s : shortcuts) {
        if (!s.executable_path.empty()) {
            found_valid_exe = true;
            TEST_CHECK(!s.id.empty());
            TEST_CHECK(!s.name.empty());
            break;
        }
    }
    TEST_CHECK(found_valid_exe);

    std::cout << "test_win_lnk passed!" << std::endl;
    return 0;
#endif
}
