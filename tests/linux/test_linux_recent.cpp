#include "src/linux/linux_recent_service.h"
#include "tests/test_common.h"

#include <filesystem>
#include <iostream>

int main() {
#ifndef __linux__
    TEST_SKIP("Linux-only test");
#else
    using namespace broapps::linux_backend;

    std::filesystem::path temp_xbel = "/tmp/broapps_test_recently_used.xbel";
    std::error_code ec;
    std::filesystem::remove(temp_xbel, ec);

    LinuxRecentService service(temp_xbel.string());

    // Initially empty
    auto initial_items = service.get_recent_items();
    TEST_CHECK(initial_items.empty());

    // Add items
    service.add_recent_item("/tmp/doc1.txt", "TextEditor");
    service.add_recent_item("/tmp/doc2.pdf", "PDFViewer");
    service.add_recent_item("/tmp/doc3.png", "ImageViewer");

    auto items = service.get_recent_items();
    TEST_CHECK_EQ(items.size(), 3u);

    // Most recent is doc3.png
    TEST_CHECK_EQ(items[0].display_name, "doc3.png");
    TEST_CHECK_EQ(items[0].app_id, "ImageViewer");
    TEST_CHECK_EQ(items[0].mime_type, "image/png");

    // Add duplicate doc1.txt (should move to top)
    service.add_recent_item("/tmp/doc1.txt", "TextEditor");
    auto updated = service.get_recent_items();
    TEST_CHECK_EQ(updated.size(), 3u);
    TEST_CHECK_EQ(updated[0].display_name, "doc1.txt");

    // Clear items
    service.clear_recent_items();
    auto cleared = service.get_recent_items();
    TEST_CHECK(cleared.empty());

    // Cleanup
    std::filesystem::remove(temp_xbel, ec);

    std::cout << "test_linux_recent passed!" << std::endl;
    return 0;
#endif
}
