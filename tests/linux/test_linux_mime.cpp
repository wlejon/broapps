#include "broapps/app_catalog.h"
#include "broapps/mime_service.h"
#include "tests/test_common.h"

#include <array>
#include <cstdio>
#include <iostream>

#ifdef __linux__
static std::string query_xdg_mime_default(const std::string& mime) {
    std::string cmd = "xdg-mime query default " + mime + " 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return {};

    std::array<char, 256> buffer;
    std::string result;
    if (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        result = buffer.data();
        while (!result.empty() && (result.back() == '\r' || result.back() == '\n')) {
            result.pop_back();
        }
    }
    pclose(pipe);
    return result;
}
#endif

int main() {
#ifndef __linux__
    TEST_SKIP("Linux-only test");
#else
    using namespace broapps;

    std::shared_ptr<AppCatalog> catalog = AppCatalog::create({{}, true});
    TEST_CHECK(catalog != nullptr);

    auto mime_svc = MimeService::create(catalog);
    TEST_CHECK(mime_svc != nullptr);

    // Test 1: Extension to MIME
    TEST_CHECK_EQ(mime_svc->extension_to_mime(".html"), "text/html");
    TEST_CHECK_EQ(mime_svc->extension_to_mime(".txt"), "text/plain");

    // Test 2: Differential check with xdg-mime query default
    std::string xdg_default = query_xdg_mime_default("text/html");
    std::cout << "xdg-mime query default text/html: " << xdg_default << std::endl;

    auto def_app = mime_svc->get_default_app_for_mime("text/html");
    if (def_app) {
        std::cout << "MimeService default app for text/html: " << def_app->id
                  << " (" << def_app->name << ")" << std::endl;
        if (!xdg_default.empty()) {
            TEST_CHECK_EQ(def_app->id, xdg_default);
        }
    }

    // Test 3: Candidates
    auto cands = mime_svc->get_candidates_for_mime("text/plain");
    std::cout << "Found " << cands.size() << " candidates for text/plain" << std::endl;

    std::cout << "test_linux_mime passed!" << std::endl;
    return 0;
#endif
}
