#include "broapps/app_catalog.h"
#include "broapps/mime_service.h"
#include "tests/test_common.h"

#include <iostream>

int main() {
#ifndef _WIN32
    TEST_SKIP("Windows-only test");
#else
    using namespace broapps;

    std::shared_ptr<AppCatalog> catalog = AppCatalog::create({{}, true});
    TEST_CHECK(catalog != nullptr);

    auto mime_svc = MimeService::create(catalog);
    TEST_CHECK(mime_svc != nullptr);

    // Test 1: Extension to MIME
    std::string txt_mime = mime_svc->extension_to_mime(".txt");
    TEST_CHECK_EQ(txt_mime, "text/plain");

    std::string png_mime = mime_svc->extension_to_mime(".png");
    TEST_CHECK_EQ(png_mime, "image/png");

    // Test 2: Default app for file / mime
    auto txt_app = mime_svc->get_default_app_for_file("example.txt");
    TEST_CHECK(txt_app.has_value());
    std::cout << "Default app for .txt: " << txt_app->name
              << " (" << txt_app->executable_path << ")" << std::endl;
    TEST_CHECK(!txt_app->name.empty() || !txt_app->executable_path.empty());

    // Test 3: Candidates for file
    auto txt_cands = mime_svc->get_candidates_for_file("example.txt");
    std::cout << "Found " << txt_cands.size() << " candidates for .txt" << std::endl;
    TEST_CHECK(!txt_cands.empty());

    std::cout << "test_win_mime passed!" << std::endl;
    return 0;
#endif
}
