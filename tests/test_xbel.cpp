#include "src/linux/xbel_parser.h"
#include "test_common.h"

#include <iostream>

int main() {
    using namespace broapps;
    using namespace broapps::linux_backend;

    std::string sample_xbel = R"(<?xml version="1.0" encoding="UTF-8"?>
<xbel version="1.0"
      xmlns:bookmark="http://www.freedesktop.org/standards/desktop-bookmarks"
      xmlns:mime="http://www.freedesktop.org/standards/shared-mime-info">
  <bookmark href="file:///home/user/Documents/report.pdf" added="2026-10-05T01:00:00Z" modified="2026-10-05T01:30:00Z" visited="2026-10-05T01:30:00Z">
    <title>report.pdf</title>
    <info>
      <metadata owner="http://freedesktop.org">
        <mime:mime-type type="application/pdf"/>
        <bookmark:applications>
          <bookmark:application name="Evince" exec="&apos;evince %u&apos;" modified="2026-10-05T01:30:00Z" count="1"/>
        </bookmark:applications>
      </metadata>
    </info>
  </bookmark>
  <bookmark href="file:///home/user/Pictures/photo.png" added="2026-10-05T00:30:00Z" modified="2026-10-05T00:45:00Z" visited="2026-10-05T00:45:00Z">
    <title>photo.png</title>
    <info>
      <metadata owner="http://freedesktop.org">
        <mime:mime-type type="image/png"/>
        <bookmark:applications>
          <bookmark:application name="GIMP" exec="&apos;gimp %u&apos;" modified="2026-10-05T00:45:00Z" count="2"/>
        </bookmark:applications>
      </metadata>
    </info>
  </bookmark>
</xbel>
)";

    // Test 1: Parse sample XBEL
    auto items = parse_xbel_string(sample_xbel);
    TEST_CHECK_EQ(items.size(), 2u);

    TEST_CHECK_EQ(items[0].display_name, "report.pdf");
    TEST_CHECK_EQ(items[0].mime_type, "application/pdf");
    TEST_CHECK_EQ(items[0].app_id, "Evince");
    TEST_CHECK(items[0].uri.find("report.pdf") != std::string::npos);

    TEST_CHECK_EQ(items[1].display_name, "photo.png");
    TEST_CHECK_EQ(items[1].mime_type, "image/png");
    TEST_CHECK_EQ(items[1].app_id, "GIMP");
    TEST_CHECK(items[1].uri.find("photo.png") != std::string::npos);

    // Test 2: Serialize and round-trip
    std::string serialized = serialize_xbel_string(items);
    auto roundtrip = parse_xbel_string(serialized);

    TEST_CHECK_EQ(roundtrip.size(), 2u);
    TEST_CHECK_EQ(roundtrip[0].display_name, "report.pdf");
    TEST_CHECK_EQ(roundtrip[0].mime_type, "application/pdf");
    TEST_CHECK_EQ(roundtrip[0].app_id, "Evince");
    TEST_CHECK_EQ(roundtrip[1].display_name, "photo.png");
    TEST_CHECK_EQ(roundtrip[1].mime_type, "image/png");
    TEST_CHECK_EQ(roundtrip[1].app_id, "GIMP");

    std::cout << "test_xbel passed!" << std::endl;
    return 0;
}
