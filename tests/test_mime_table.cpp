#include "src/common/mime_table.h"
#include "test_common.h"

#include <algorithm>
#include <iostream>

int main() {
    using namespace broapps;

    // Test 1: Common extensions
    TEST_CHECK_EQ(lookup_mime_by_extension(".txt"), "text/plain");
    TEST_CHECK_EQ(lookup_mime_by_extension("txt"), "text/plain");
    TEST_CHECK_EQ(lookup_mime_by_extension("HTML"), "text/html");
    TEST_CHECK_EQ(lookup_mime_by_extension(".png"), "image/png");
    TEST_CHECK_EQ(lookup_mime_by_extension(".jpg"), "image/jpeg");
    TEST_CHECK_EQ(lookup_mime_by_extension(".pdf"), "application/pdf");
    TEST_CHECK_EQ(lookup_mime_by_extension(".json"), "application/json");
    TEST_CHECK_EQ(lookup_mime_by_extension(".mp3"), "audio/mpeg");
    TEST_CHECK_EQ(lookup_mime_by_extension(".mp4"), "video/mp4");
    TEST_CHECK_EQ(lookup_mime_by_extension(".zip"), "application/zip");

    // Test 2: Unknown extension fallback
    TEST_CHECK_EQ(lookup_mime_by_extension(".unknown_xyz_ext_123"), "application/octet-stream");

    // Test 3: Reverse lookup (MIME to extensions)
    auto exts_png = lookup_extensions_by_mime("image/png");
    TEST_CHECK(!exts_png.empty());
    TEST_CHECK(std::find(exts_png.begin(), exts_png.end(), ".png") != exts_png.end());

    auto exts_html = lookup_extensions_by_mime("text/html");
    TEST_CHECK(!exts_html.empty());
    TEST_CHECK(std::find(exts_html.begin(), exts_html.end(), ".html") != exts_html.end());

    std::cout << "test_mime_table passed!" << std::endl;
    return 0;
}
