#include "broapps/icon_resolver.h"
#include "tests/test_common.h"

#include <filesystem>
#include <fstream>
#include <iostream>

#ifdef __linux__
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace {

void touch(const fs::path& p) {
    fs::create_directories(p.parent_path());
    std::ofstream(p) << "icon";
}

}  // namespace

int main() {
#ifndef __linux__
    TEST_SKIP("Linux-only test");
#else
    using namespace broapps;

    // A private icon tree the resolver finds through XDG_DATA_HOME / XDG_DATA_DIRS, so the
    // test asserts exact answers instead of depending on which themes the machine has.
    const fs::path root = fs::temp_directory_path() / ("broapps-icon-test-" + std::to_string(getpid()));
    std::error_code ec;
    fs::remove_all(root, ec);
    const fs::path home_icons = root / "home" / "icons";
    const fs::path data_icons = root / "data" / "icons";

    touch(home_icons / "hicolor" / "48x48" / "apps" / "broapps-sized.png");
    touch(home_icons / "hicolor" / "32x32" / "apps" / "broapps-sized.png");
    touch(home_icons / "hicolor" / "scalable" / "apps" / "broapps-scalable.svg");
    touch(data_icons / "hicolor" / "48x48" / "apps" / "broapps-shadowed.png");
    touch(home_icons / "hicolor" / "48x48" / "apps" / "broapps-shadowed.png");
    touch(data_icons / "Adwaita" / "64x64" / "apps" / "broapps-themed.png");
    touch(data_icons / "broapps-flat.xpm");
    touch(root / "absolute" / "broapps-abs.png");

    setenv("XDG_DATA_HOME", (root / "home").c_str(), 1);
    setenv("XDG_DATA_DIRS", (root / "data").c_str(), 1);

    auto resolver = IconResolver::create();
    TEST_CHECK(resolver != nullptr);

    // The requested size wins over other sizes of the same icon.
    auto sized = resolver->resolve_icon("broapps-sized", 48);
    TEST_CHECK(sized.has_value());
    TEST_CHECK_EQ(*sized, home_icons / "hicolor" / "48x48" / "apps" / "broapps-sized.png");
    auto sized32 = resolver->resolve_icon("broapps-sized", 32);
    TEST_CHECK(sized32.has_value());
    TEST_CHECK_EQ(*sized32, home_icons / "hicolor" / "32x32" / "apps" / "broapps-sized.png");

    // Scalable when no bitmap size exists.
    auto scalable = resolver->resolve_icon("broapps-scalable", 48);
    TEST_CHECK(scalable.has_value());
    TEST_CHECK_EQ(*scalable, home_icons / "hicolor" / "scalable" / "apps" / "broapps-scalable.svg");

    // XDG_DATA_HOME shadows XDG_DATA_DIRS.
    auto shadowed = resolver->resolve_icon("broapps-shadowed", 48);
    TEST_CHECK(shadowed.has_value());
    TEST_CHECK_EQ(*shadowed, home_icons / "hicolor" / "48x48" / "apps" / "broapps-shadowed.png");

    // A non-hicolor theme, and an unthemed icon at the top of an icons dir.
    auto themed = resolver->resolve_icon("broapps-themed", 48);
    TEST_CHECK(themed.has_value());
    TEST_CHECK_EQ(*themed, data_icons / "Adwaita" / "64x64" / "apps" / "broapps-themed.png");
    auto flat = resolver->resolve_icon("broapps-flat", 48);
    TEST_CHECK(flat.has_value());
    TEST_CHECK_EQ(*flat, data_icons / "broapps-flat.xpm");

    // An absolute path is taken as is; a missing one, or an unknown name, resolves to nothing.
    auto abs = resolver->resolve_icon((root / "absolute" / "broapps-abs.png").string(), 48);
    TEST_CHECK(abs.has_value());
    TEST_CHECK_EQ(*abs, root / "absolute" / "broapps-abs.png");
    TEST_CHECK(!resolver->resolve_icon((root / "absolute" / "missing.png").string(), 48).has_value());
    TEST_CHECK(!resolver->resolve_icon("broapps-no-such-icon", 48).has_value());
    TEST_CHECK(!resolver->resolve_icon("", 48).has_value());

    fs::remove_all(root, ec);

    // The machine's own themes, for the log: what resolves depends on what is installed
    // (a server or CI runner has almost nothing), so each answer is only checked to exist.
    unsetenv("XDG_DATA_HOME");
    unsetenv("XDG_DATA_DIRS");
    auto system = IconResolver::create();
    TEST_CHECK(system != nullptr);
    for (const char* name : {"utilities-terminal", "system-search", "text-editor", "cmake", "btop"}) {
        auto icon_path = system->resolve_icon(name, 48);
        if (icon_path) {
            std::cout << "Resolved icon '" << name << "' -> " << *icon_path << std::endl;
            TEST_CHECK(fs::exists(*icon_path));
        }
    }

    std::cout << "test_linux_icon passed!" << std::endl;
    return 0;
#endif
}
