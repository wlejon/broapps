#include "src/common/catalog_base.h"
#include "test_common.h"

namespace broapps {

class MockCatalog : public CatalogBase {
public:
    explicit MockCatalog(CatalogConfig cfg, std::vector<AppInfo> apps)
        : CatalogBase(std::move(cfg)) {
        set_apps(std::move(apps));
    }
    void refresh() override {}
};

}  // namespace broapps

int main() {
    using namespace broapps;

    std::vector<AppInfo> sample_apps;

    AppInfo term;
    term.id = "org.gnome.Terminal.desktop";
    term.name = "Terminal";
    term.generic_name = "Terminal Emulator";
    term.comment = "Use the command line";
    term.executable_path = "/usr/bin/gnome-terminal";
    term.categories = {"System", "TerminalEmulator"};
    term.keywords = {"shell", "prompt", "command"};
    sample_apps.push_back(term);

    AppInfo ff;
    ff.id = "firefox.desktop";
    ff.name = "Firefox Web Browser";
    ff.generic_name = "Web Browser";
    ff.comment = "Browse the World Wide Web";
    ff.executable_path = "/usr/bin/firefox";
    ff.categories = {"Network", "WebBrowser"};
    ff.keywords = {"internet", "web", "browser"};
    ff.supported_mime_types = {"text/html", "application/xhtml+xml"};
    sample_apps.push_back(ff);

    AppInfo gimp;
    gimp.id = "gimp.desktop";
    gimp.name = "GNU Image Manipulation Program";
    gimp.generic_name = "Image Editor";
    gimp.comment = "Create images and edit photographs";
    gimp.executable_path = "/usr/bin/gimp-2.10";
    gimp.categories = {"Graphics", "2DGraphics", "RasterGraphics"};
    gimp.keywords = {"image", "photo", "paint"};
    gimp.supported_mime_types = {"image/png", "image/jpeg", "image/bmp"};
    sample_apps.push_back(gimp);

    AppInfo hidden;
    hidden.id = "hidden-helper.desktop";
    hidden.name = "Hidden Service";
    hidden.is_nodisplay = true;
    sample_apps.push_back(hidden);

    // Test 1: find_by_id
    {
        MockCatalog cat({}, sample_apps);
        auto a1 = cat.find_by_id("firefox.desktop");
        TEST_CHECK(a1.has_value());
        TEST_CHECK_EQ(a1->name, "Firefox Web Browser");

        auto a2 = cat.find_by_id("nonexistent");
        TEST_CHECK(!a2.has_value());
    }

    // Test 2: Search ranking
    {
        MockCatalog cat({}, sample_apps);

        // Exact match should be #1
        auto r_term = cat.search("Terminal");
        TEST_CHECK(!r_term.empty());
        TEST_CHECK_EQ(r_term[0].id, "org.gnome.Terminal.desktop");

        // Keyword match
        auto r_shell = cat.search("shell");
        TEST_CHECK(!r_shell.empty());
        TEST_CHECK_EQ(r_shell[0].id, "org.gnome.Terminal.desktop");

        // Generic name match
        auto r_editor = cat.search("Image Editor");
        TEST_CHECK(!r_editor.empty());
        TEST_CHECK_EQ(r_editor[0].id, "gimp.desktop");

        // Web search matches Firefox
        auto r_web = cat.search("web");
        TEST_CHECK(!r_web.empty());
        TEST_CHECK_EQ(r_web[0].id, "firefox.desktop");
    }

    // Test 3: NoDisplay exclusion
    {
        MockCatalog cat({}, sample_apps);
        auto res = cat.search("Hidden");
        TEST_CHECK(res.empty());

        CatalogConfig cfg;
        cfg.include_nodisplay = true;
        MockCatalog cat_show(cfg, sample_apps);
        auto res_show = cat_show.search("Hidden");
        TEST_CHECK_EQ(res_show.size(), 1u);
        TEST_CHECK_EQ(res_show[0].id, "hidden-helper.desktop");
    }

    // Test 4: Category search
    {
        MockCatalog cat({}, sample_apps);
        auto graphics = cat.find_by_category("Graphics");
        TEST_CHECK_EQ(graphics.size(), 1u);
        TEST_CHECK_EQ(graphics[0].id, "gimp.desktop");

        auto net = cat.find_by_category("network");
        TEST_CHECK_EQ(net.size(), 1u);
        TEST_CHECK_EQ(net[0].id, "firefox.desktop");
    }

    // Test 5: MIME type matching
    {
        MockCatalog cat({}, sample_apps);
        auto html_apps = cat.find_by_mime_type("text/html");
        TEST_CHECK_EQ(html_apps.size(), 1u);
        TEST_CHECK_EQ(html_apps[0].id, "firefox.desktop");

        auto img_apps = cat.find_by_mime_type("image/png");
        TEST_CHECK_EQ(img_apps.size(), 1u);
        TEST_CHECK_EQ(img_apps[0].id, "gimp.desktop");

        // Wildcard
        auto all_imgs = cat.find_by_mime_type("image/*");
        TEST_CHECK_EQ(all_imgs.size(), 1u);
        TEST_CHECK_EQ(all_imgs[0].id, "gimp.desktop");
    }

    std::cout << "test_search passed!" << std::endl;
    return 0;
}
