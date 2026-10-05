#include "broapps/app_catalog.h"
#include "broapps/mime_service.h"
#include "tests/test_common.h"

#include <iostream>

int main() {
#ifndef __APPLE__
    TEST_SKIP("macOS-only test");
#else
    using namespace broapps;

    std::shared_ptr<AppCatalog> catalog = AppCatalog::create({{}, true});
    TEST_CHECK(catalog != nullptr);

    auto mime_svc = MimeService::create(catalog);
    TEST_CHECK(mime_svc != nullptr);

    // Test 1: Extension to MIME
    TEST_CHECK_EQ(mime_svc->extension_to_mime(".txt"), "text/plain");

    // Test 2: Default app for HTML
    auto html_app = mime_svc->get_default_app_for_mime("text/html");
    if (html_app) {
        std::cout << "macOS default app for text/html: " << html_app->name
                  << " (" << html_app->bundle_id << ")" << std::endl;
        TEST_CHECK(!html_app->name.empty() || !html_app->bundle_id.empty());
    }

    // Test 3: Candidates
    auto txt_cands = mime_svc->get_candidates_for_mime("text/plain");
    std::cout << "Found " << txt_cands.size() << " candidates for text/plain on macOS." << std::endl;

    std::cout << "test_mac_mime passed!" << std::endl;
    return 0;
#endif
}
