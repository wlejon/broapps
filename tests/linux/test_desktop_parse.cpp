#include "src/linux/desktop_entry.h"
#include "tests/test_common.h"

#include <iostream>

int main() {
    using namespace broapps::linux_backend;

    // Test 1: Full desktop entry parse
    std::string sample = R"(
[Desktop Entry]
Version=1.5
Type=Application
Name=GIMP\sImage\sEditor
GenericName=Image Editor
Comment=Create images and edit photographs
Exec=gimp-2.10 %F
Icon=gimp
Terminal=false
Categories=Graphics;2DGraphics;RasterGraphics;
Keywords=image;photo;paint;
MimeType=image/bmp;image/g3fax;image/gif;
Actions=NewWindow;

[Desktop Action NewWindow]
Name=Open a New Window
Exec=gimp-2.10 --new-instance %U
Icon=gimp-new-window
)";

    auto app = parse_desktop_entry_string(sample, "/usr/share/applications/gimp.desktop", "gimp.desktop");
    TEST_CHECK(app.has_value());
    TEST_CHECK_EQ(app->id, "gimp.desktop");
    TEST_CHECK_EQ(app->name, "GIMP Image Editor");
    TEST_CHECK_EQ(app->generic_name, "Image Editor");
    TEST_CHECK_EQ(app->comment, "Create images and edit photographs");
    TEST_CHECK_EQ(app->icon_name_or_path, "gimp");
    TEST_CHECK(!app->is_terminal);
    TEST_CHECK(!app->is_nodisplay);
    TEST_CHECK_EQ(app->categories.size(), 3u);
    TEST_CHECK_EQ(app->categories[0], "Graphics");
    TEST_CHECK_EQ(app->keywords.size(), 3u);
    TEST_CHECK_EQ(app->supported_mime_types.size(), 3u);
    TEST_CHECK_EQ(app->actions.size(), 1u);
    TEST_CHECK_EQ(app->actions[0].id, "NewWindow");
    TEST_CHECK_EQ(app->actions[0].name, "Open a New Window");

    // Test 2: Field code expansion (%F)
    {
        broapps::LaunchScope scope;
        scope.files_to_open = {"/home/user/photo1.png", "/home/user/photo2.jpg"};
        auto argv = expand_exec(*app, scope);

        TEST_CHECK_EQ(argv.size(), 3u);
        TEST_CHECK_EQ(argv[0], "gimp-2.10");
        TEST_CHECK_EQ(argv[1], "/home/user/photo1.png");
        TEST_CHECK_EQ(argv[2], "/home/user/photo2.jpg");
    }

    // Test 3: Action execution with %U
    {
        broapps::LaunchScope scope;
        scope.action_id = "NewWindow";
        scope.files_to_open = {"https://example.com/art.png"};
        auto argv = expand_exec(*app, scope);

        TEST_CHECK_EQ(argv.size(), 3u);
        TEST_CHECK_EQ(argv[0], "gimp-2.10");
        TEST_CHECK_EQ(argv[1], "--new-instance");
        TEST_CHECK_EQ(argv[2], "https://example.com/art.png");
    }

    // Test 4: Quoting, escaping, %i, %c, %k, %%
    {
        std::string complex_entry = R"(
[Desktop Entry]
Type=Application
Name=Complex App
Icon=app-icon
Exec=complex-tool --title "%c" %i --desktop-file "%k" --ratio 100%% %f "hello \"world\""
)";
        auto c_app = parse_desktop_entry_string(complex_entry, "/usr/share/applications/complex.desktop", "complex.desktop");
        TEST_CHECK(c_app.has_value());

        broapps::LaunchScope scope;
        scope.files_to_open = {"/tmp/test.txt"};
        auto argv = expand_exec(*c_app, scope);

        TEST_CHECK_EQ(argv[0], "complex-tool");
        TEST_CHECK_EQ(argv[1], "--title");
        TEST_CHECK_EQ(argv[2], "Complex App");
        TEST_CHECK_EQ(argv[3], "--icon");
        TEST_CHECK_EQ(argv[4], "app-icon");
        TEST_CHECK_EQ(argv[5], "--desktop-file");
        TEST_CHECK_EQ(argv[6], "/usr/share/applications/complex.desktop");
        TEST_CHECK_EQ(argv[7], "--ratio");
        TEST_CHECK_EQ(argv[8], "100%");
        TEST_CHECK_EQ(argv[9], "/tmp/test.txt");
        TEST_CHECK_EQ(argv[10], "hello \"world\"");
    }

    // Test 5: Ignored non-application types
    {
        std::string dir_entry = R"(
[Desktop Entry]
Type=Directory
Name=Submenu
)";
        auto d_app = parse_desktop_entry_string(dir_entry);
        TEST_CHECK(!d_app.has_value());
    }

    std::cout << "test_desktop_parse passed!" << std::endl;
    return 0;
}
