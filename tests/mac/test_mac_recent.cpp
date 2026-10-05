#include "src/mac/mac_recent_service.h"
#include "tests/test_common.h"

#include <filesystem>
#include <iostream>

int main() {
#ifndef __APPLE__
    TEST_SKIP("macOS-only test");
#else
    using namespace broapps::mac_backend;

    std::filesystem::path temp_plist = "/tmp/broapps_test_recent_mac.plist";
    std::error_code ec;
    std::filesystem::remove(temp_plist, ec);

    MacRecentService service(temp_plist.string());

    // Initially empty
    auto initial_items = service.get_recent_items();
    TEST_CHECK(initial_items.empty());

    // Add items
    service.add_recent_item("/tmp/doc1.txt", "TextEdit");
    service.add_recent_item("/tmp/doc2.pdf", "Preview");
    service.add_recent_item("/tmp/doc3.png", "Preview");

    auto items = service.get_recent_items();
    TEST_CHECK_EQ(items.size(), 3u);
    TEST_CHECK_EQ(items[0].display_name, "doc3.png");
    TEST_CHECK_EQ(items[0].mime_type, "image/png");

    // Add duplicate doc1.txt (should move to top)
    service.add_recent_item("/tmp/doc1.txt", "TextEdit");
    auto updated = service.get_recent_items();
    TEST_CHECK_EQ(updated.size(), 3u);
    TEST_CHECK_EQ(updated[0].display_name, "doc1.txt");

    // Clear items
    service.clear_recent_items();
    auto cleared = service.get_recent_items();
    TEST_CHECK(cleared.empty());

    // Cleanup
    std::filesystem::remove(temp_plist, ec);

    std::cout << "test_mac_recent passed!" << std::endl;
    return 0;
#endif
}
