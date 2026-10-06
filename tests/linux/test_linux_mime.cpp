#include "broapps/app_catalog.h"
#include "broapps/mime_service.h"
#include "tests/test_common.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

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

    // Test 4: set_default_app_for_mime in an isolated XDG_CONFIG_HOME
    std::cout << "Testing set_default_app_for_mime with isolated XDG_CONFIG_HOME..." << std::endl;
    {
        const char* orig_config_home = std::getenv("XDG_CONFIG_HOME");
        std::string orig_config_str = orig_config_home ? orig_config_home : "";

        auto temp_base = std::filesystem::temp_directory_path() /
                         ("broapps_test_linux_mime_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        auto temp_config = temp_base / "config";
        auto temp_apps = temp_base / "apps";
        std::filesystem::create_directories(temp_config);
        std::filesystem::create_directories(temp_apps);

        // Create two sample desktop files
        {
            std::ofstream ed(temp_apps / "test-text-editor.desktop");
            ed << "[Desktop Entry]\n"
               << "Type=Application\n"
               << "Name=Test Text Editor\n"
               << "Exec=/bin/echo editor\n"
               << "MimeType=text/plain;\n";
        }
        {
            std::ofstream vw(temp_apps / "test-text-viewer.desktop");
            vw << "[Desktop Entry]\n"
               << "Type=Application\n"
               << "Name=Test Text Viewer\n"
               << "Exec=/bin/echo viewer\n"
               << "MimeType=text/plain;image/png;\n";
        }

        setenv("XDG_CONFIG_HOME", temp_config.c_str(), 1);

        CatalogConfig cat_cfg;
        cat_cfg.extra_search_paths = {temp_apps.string()};
        cat_cfg.include_nodisplay = true;
        std::shared_ptr<AppCatalog> iso_catalog = AppCatalog::create(cat_cfg);
        TEST_CHECK(iso_catalog != nullptr);

        auto iso_mime = MimeService::create(iso_catalog);
        TEST_CHECK(iso_mime != nullptr);

        // 4a. Set default app for text/plain
        bool ok = iso_mime->set_default_app_for_mime("text/plain", "test-text-editor.desktop");
        TEST_CHECK(ok);

        auto def1 = iso_mime->get_default_app_for_mime("text/plain");
        TEST_CHECK(def1.has_value());
        TEST_CHECK_EQ(def1->id, "test-text-editor.desktop");

        // Candidates should list the default first
        auto cands1 = iso_mime->get_candidates_for_mime("text/plain");
        TEST_CHECK(!cands1.empty());
        TEST_CHECK_EQ(cands1.front().id, "test-text-editor.desktop");

        // Verify mimeapps.list on disk
        auto list_path = temp_config / "mimeapps.list";
        TEST_CHECK(std::filesystem::exists(list_path));
        {
            std::ifstream in(list_path);
            std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            TEST_CHECK(content.find("[Default Applications]") != std::string::npos);
            TEST_CHECK(content.find("text/plain=test-text-editor.desktop") != std::string::npos);
        }

        // 4b. Update default app for text/plain to test-text-viewer.desktop
        ok = iso_mime->set_default_app_for_mime("text/plain", "test-text-viewer.desktop");
        TEST_CHECK(ok);

        auto def2 = iso_mime->get_default_app_for_mime("text/plain");
        TEST_CHECK(def2.has_value());
        TEST_CHECK_EQ(def2->id, "test-text-viewer.desktop");

        // Verify file updated without duplicating text/plain
        {
            std::ifstream in(list_path);
            std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            TEST_CHECK(content.find("text/plain=test-text-viewer.desktop") != std::string::npos);
            TEST_CHECK(content.find("text/plain=test-text-editor.desktop") == std::string::npos);
        }

        // 4c. Add another mime type: image/png
        ok = iso_mime->set_default_app_for_mime("image/png", "test-text-viewer.desktop");
        TEST_CHECK(ok);

        auto def_png = iso_mime->get_default_app_for_mime("image/png");
        TEST_CHECK(def_png.has_value());
        TEST_CHECK_EQ(def_png->id, "test-text-viewer.desktop");

        // Verify both entries in file
        {
            std::ifstream in(list_path);
            std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            TEST_CHECK(content.find("text/plain=test-text-viewer.desktop") != std::string::npos);
            TEST_CHECK(content.find("image/png=test-text-viewer.desktop") != std::string::npos);
        }

        // 4d. Fresh MimeService loads persisted file correctly
        auto fresh_mime = MimeService::create(iso_catalog);
        auto fresh_def_txt = fresh_mime->get_default_app_for_mime("text/plain");
        TEST_CHECK(fresh_def_txt.has_value());
        TEST_CHECK_EQ(fresh_def_txt->id, "test-text-viewer.desktop");

        auto fresh_def_png = fresh_mime->get_default_app_for_mime("image/png");
        TEST_CHECK(fresh_def_png.has_value());
        TEST_CHECK_EQ(fresh_def_png->id, "test-text-viewer.desktop");

        // 4e. Verify other sections and comments in mimeapps.list are preserved
        {
            std::ofstream out(list_path);
            out << "# Leading comment\n"
                << "[Added Associations]\n"
                << "text/plain=test-text-editor.desktop;\n"
                << "\n"
                << "[Default Applications]\n"
                << "text/plain=test-text-editor.desktop\n"
                << "\n"
                << "[Removed Associations]\n"
                << "text/plain=bad.desktop;\n";
        }
        ok = iso_mime->set_default_app_for_mime("text/plain", "test-text-viewer.desktop");
        TEST_CHECK(ok);

        {
            std::ifstream in(list_path);
            std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            TEST_CHECK(content.find("# Leading comment") != std::string::npos);
            TEST_CHECK(content.find("[Added Associations]") != std::string::npos);
            TEST_CHECK(content.find("text/plain=test-text-editor.desktop;") != std::string::npos);
            TEST_CHECK(content.find("[Default Applications]") != std::string::npos);
            TEST_CHECK(content.find("text/plain=test-text-viewer.desktop") != std::string::npos);
            TEST_CHECK(content.find("[Removed Associations]") != std::string::npos);
            TEST_CHECK(content.find("text/plain=bad.desktop;") != std::string::npos);
        }

        // Clean up
        std::error_code ec;
        std::filesystem::remove_all(temp_base, ec);
        if (orig_config_home) {
            setenv("XDG_CONFIG_HOME", orig_config_str.c_str(), 1);
        } else {
            unsetenv("XDG_CONFIG_HOME");
        }
    }

    std::cout << "test_linux_mime passed!" << std::endl;
    return 0;
#endif
}
