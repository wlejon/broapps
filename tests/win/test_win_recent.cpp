#include "broapps/recent_service.h"
#include "tests/test_common.h"

#include <iostream>

int main() {
#ifndef _WIN32
    TEST_SKIP("Windows-only test");
#else
    using namespace broapps;

    auto recent = RecentService::create();
    TEST_CHECK(recent != nullptr);

    auto items = recent->get_recent_items(10);
    std::cout << "Retrieved " << items.size() << " recent items on Windows." << std::endl;

    for (const auto& it : items) {
        TEST_CHECK(!it.display_name.empty());
        TEST_CHECK(!it.file_path.empty());
    }

    std::cout << "test_win_recent passed!" << std::endl;
    return 0;
#endif
}
