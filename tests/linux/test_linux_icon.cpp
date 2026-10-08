#include "broapps/icon_resolver.h"
#include "linux/linux_icon_resolver.h"
#include "linux/linux_icon_theme.h"
#include "tests/test_common.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

#ifdef __linux__
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace {

void touch(const fs::path& p, const std::string& text = "icon") {
    fs::create_directories(p.parent_path());
    std::ofstream(p) << text;
}

std::optional<std::string> saved_env(const char* name) {
    const char* v = std::getenv(name);
    return v ? std::optional<std::string>(v) : std::nullopt;
}

void restore_env(const char* name, const std::optional<std::string>& v) {
#ifdef __linux__
    if (v) setenv(name, v->c_str(), 1);
    else unsetenv(name);
#else
    (void)name; (void)v;
#endif
}

#define CHECK_RESOLVES(resolver, name, opts, expected)                         \
    do {                                                                       \
        auto r_ = (resolver).resolve_icon((name), (opts));                     \
        if (!r_) {                                                             \
            std::cerr << "unresolved: " << (name) << std::endl;                \
        }                                                                      \
        TEST_CHECK(r_.has_value());                                            \
        TEST_CHECK_EQ(*r_, (expected));                                        \
    } while (0)

broapps::IconLookupOptions opt(uint32_t size, uint32_t scale = 1, std::string theme = {}) {
    broapps::IconLookupOptions o;
    o.size = size;
    o.scale = scale;
    o.theme = std::move(theme);
    return o;
}

}  // namespace

int main() {
#ifndef __linux__
    TEST_SKIP("Linux-only test");
#else
    using namespace broapps;
    using namespace broapps::linux_backend;

    const char* env_names[] = {"HOME", "XDG_DATA_HOME", "XDG_DATA_DIRS", "XDG_CONFIG_HOME",
                               "XDG_CURRENT_DESKTOP", "DESKTOP_SESSION"};
    std::optional<std::string> env_saved[std::size(env_names)];
    for (size_t i = 0; i < std::size(env_names); ++i) env_saved[i] = saved_env(env_names[i]);

    const fs::path root = fs::temp_directory_path() / ("broapps-icon-test-" + std::to_string(getpid()));
    std::error_code ec;
    fs::remove_all(root, ec);
    const fs::path b1 = root / "home" / "icons";
    const fs::path b2 = root / "data" / "icons";
    const fs::path pix = root / "pixmaps";

    // ---- Fixture themes ------------------------------------------------------------------
    // Child (b1) inherits Parent (b2), which inherits Child back: a cycle. Neither names
    // hicolor, which the lookup must still reach.
    touch(b1 / "Child" / "index.theme",
          "[Icon Theme]\n"
          "Name=Child\n"
          "Name[de]=Kind\n"
          "Inherits=Parent\n"
          "Directories=apps/48,apps/32,apps/24,apps/scalable,unlisted-section\n"
          "ScaledDirectories=apps/48@2\n"
          "\n"
          "[apps/48]\nSize=48\nType=Fixed\n"
          "[apps/32]\nSize=32\nType=Fixed\n"
          "[apps/24]\nSize=24\nType=Threshold\nThreshold=4\n"
          "[apps/scalable]\nSize=48\nType=Scalable\nMinSize=64\nMaxSize=256\n"
          "[apps/48@2]\nSize=48\nScale=2\nType=Fixed\n"
          "[apps/not-in-directories]\nSize=16\nType=Fixed\n");
    touch(b2 / "Parent" / "index.theme",
          "[Icon Theme]\nName=Parent\nInherits=Child\nDirectories=64x64/apps\n"
          "[64x64/apps]\nSize=64\nType=Fixed\n");
    touch(b2 / "hicolor" / "index.theme",
          "[Icon Theme]\nName=Hicolor\nDirectories=48x48/apps,scalable/apps\n"
          "[48x48/apps]\nSize=48\nType=Threshold\n"
          "[scalable/apps]\nSize=16\nMinSize=8\nMaxSize=512\nType=Scalable\n");
    touch(b2 / "Other" / "index.theme",
          "[Icon Theme]\nName=Other\nDirectories=48x48/apps\n[48x48/apps]\nSize=48\nType=Fixed\n");

    const fs::path c = b1 / "Child";
    // Exact size per Fixed dir, Scalable only for sizes inside MinSize..MaxSize.
    touch(c / "apps" / "48" / "exact.png");
    touch(c / "apps" / "32" / "exact.png");
    touch(c / "apps" / "scalable" / "exact.svg");
    // PNG beats SVG beats XPM in one directory.
    touch(c / "apps" / "48" / "multi.xpm");
    touch(c / "apps" / "48" / "multi.svg");
    touch(c / "apps" / "48" / "multi.png");
    touch(c / "apps" / "48" / "svgxpm.xpm");
    touch(c / "apps" / "48" / "svgxpm.svg");
    // Threshold (24 +-4) vs Fixed 32.
    touch(c / "apps" / "24" / "thr.png");
    touch(c / "apps" / "32" / "thr.png");
    // Closest among Fixed dirs only.
    touch(c / "apps" / "32" / "near.png");
    touch(c / "apps" / "48" / "near.png");
    // Scale.
    touch(c / "apps" / "48" / "hidpi.png");
    touch(c / "apps" / "48@2" / "hidpi.png");
    touch(c / "apps" / "48@2" / "hidpi2.png");
    touch(c / "apps" / "32" / "hidpi2.png");
    // Inheritance: a closer match in a parent does not beat any match in the child.
    touch(c / "apps" / "32" / "both.png");
    touch(b2 / "Parent" / "64x64" / "apps" / "both.png");
    touch(b2 / "Parent" / "64x64" / "apps" / "inherited.png");
    // A dir in the index but not in Directories= is never searched.
    touch(c / "apps" / "not-in-directories" / "hidden.png");
    // The theme split over two base dirs: the second root contributes, the first shadows.
    touch(b2 / "Child" / "apps" / "48" / "split.png");
    touch(b2 / "Child" / "apps" / "48" / "shadowed.png");
    touch(c / "apps" / "48" / "shadowed.png");
    // hicolor and unthemed fallbacks.
    touch(b2 / "hicolor" / "48x48" / "apps" / "hi.png");
    touch(b2 / "hicolor" / "scalable" / "apps" / "hi.svg");
    touch(b2 / "hicolor" / "scalable" / "apps" / "hisvg.svg");
    touch(pix / "pix.xpm");
    touch(pix / "literal.png");
    touch(b2 / "unthemed.png");
    touch(b2 / "Other" / "48x48" / "apps" / "exact.png");
    // A theme without index.theme: directories guessed from the tree.
    touch(b2 / "Guess" / "48x48" / "apps" / "g.png");
    touch(b2 / "Guess" / "16x16" / "apps" / "g.png");
    touch(b2 / "Guess" / "scalable" / "apps" / "g.svg");
    touch(b2 / "Guess" / "apps" / "22" / "g.png");
    touch(root / "absolute" / "abs.png");

    // ---- index.theme parsing --------------------------------------------------------------
    {
        IconTheme t;
        parse_index_theme(
            "# comment\n[Icon Theme]\nInherits= A , B,\nDirectories=d1,d2,d3\n"
            "[d1]\nSize=22\n[d2]\nSize=16\nType=Scalable\n[d3]\nType=Fixed\n", t);
        TEST_CHECK_EQ(t.parents.size(), 2u);
        TEST_CHECK_EQ(t.parents[0], "A");
        TEST_CHECK_EQ(t.parents[1], "B");
        TEST_CHECK_EQ(t.dirs.size(), 2u);  // d3 has no Size
        TEST_CHECK(t.dirs[0].type == IconDirType::Threshold);
        TEST_CHECK_EQ(t.dirs[0].threshold, 2);
        TEST_CHECK(t.dirs[1].type == IconDirType::Scalable);
        TEST_CHECK_EQ(t.dirs[1].min_size, 16);  // MinSize/MaxSize default to Size
        TEST_CHECK_EQ(t.dirs[1].max_size, 16);

        IconThemeDir fixed{"f", IconDirType::Fixed, 32, 1, 32, 32, 2, {}};
        TEST_CHECK(icon_dir_matches_size(fixed, 32, 1));
        TEST_CHECK(!icon_dir_matches_size(fixed, 33, 1));
        TEST_CHECK(!icon_dir_matches_size(fixed, 32, 2));
        TEST_CHECK_EQ(icon_dir_size_distance(fixed, 40, 1), 8);
        TEST_CHECK_EQ(icon_dir_size_distance(fixed, 16, 2), 0);
        IconThemeDir thr{"t", IconDirType::Threshold, 24, 1, 24, 24, 4, {}};
        TEST_CHECK(icon_dir_matches_size(thr, 20, 1));
        TEST_CHECK(icon_dir_matches_size(thr, 28, 1));
        TEST_CHECK(!icon_dir_matches_size(thr, 29, 1));
        TEST_CHECK_EQ(icon_dir_size_distance(thr, 30, 1), 2);
        TEST_CHECK_EQ(icon_dir_size_distance(thr, 16, 1), 4);
        IconThemeDir sc{"s", IconDirType::Scalable, 48, 2, 16, 256, 2, {}};
        TEST_CHECK_EQ(icon_dir_size_distance(sc, 16, 1), 16);  // 32 - 16
        TEST_CHECK_EQ(icon_dir_size_distance(sc, 600, 1), 88);
    }

    // ---- Lookup against the fixture, explicit roots --------------------------------------
    LinuxIconResolver r({b1, b2, pix}, "Child");
    TEST_CHECK_EQ(r.default_theme(), "Child");

    CHECK_RESOLVES(r, "exact", opt(48), c / "apps" / "48" / "exact.png");
    CHECK_RESOLVES(r, "exact", opt(32), c / "apps" / "32" / "exact.png");
    CHECK_RESOLVES(r, "exact", opt(128), c / "apps" / "scalable" / "exact.svg");
    // 40: nothing matches; Fixed 32 and 48 are both 8 away, Scalable (64..256) 24 away;
    // the tie goes to the directory listed first.
    CHECK_RESOLVES(r, "exact", opt(40), c / "apps" / "48" / "exact.png");
    CHECK_RESOLVES(r, "near", opt(36), c / "apps" / "32" / "near.png");
    CHECK_RESOLVES(r, "near", opt(44), c / "apps" / "48" / "near.png");
    CHECK_RESOLVES(r, "near", opt(16), c / "apps" / "32" / "near.png");

    CHECK_RESOLVES(r, "multi", opt(48), c / "apps" / "48" / "multi.png");
    CHECK_RESOLVES(r, "svgxpm", opt(48), c / "apps" / "48" / "svgxpm.svg");

    CHECK_RESOLVES(r, "thr", opt(27), c / "apps" / "24" / "thr.png");   // inside 24 +- 4
    CHECK_RESOLVES(r, "thr", opt(32), c / "apps" / "32" / "thr.png");   // Fixed exact
    CHECK_RESOLVES(r, "thr", opt(30), c / "apps" / "32" / "thr.png");   // 2 from 28, 2 from 32: listed first
    CHECK_RESOLVES(r, "thr", opt(31), c / "apps" / "32" / "thr.png");   // 3 from 28 vs 1 from 32

    CHECK_RESOLVES(r, "hidpi", opt(48, 1), c / "apps" / "48" / "hidpi.png");
    CHECK_RESOLVES(r, "hidpi", opt(48, 2), c / "apps" / "48@2" / "hidpi.png");
    CHECK_RESOLVES(r, "hidpi2", opt(48, 1), c / "apps" / "32" / "hidpi2.png");   // 96 is 48 away, 32 is 16
    CHECK_RESOLVES(r, "hidpi2", opt(96, 1), c / "apps" / "48@2" / "hidpi2.png"); // 48@2 is 96 px

    CHECK_RESOLVES(r, "both", opt(64), c / "apps" / "32" / "both.png");
    CHECK_RESOLVES(r, "inherited", opt(48), b2 / "Parent" / "64x64" / "apps" / "inherited.png");
    TEST_CHECK(!r.resolve_icon("hidden", opt(16)).has_value());
    CHECK_RESOLVES(r, "split", opt(48), b2 / "Child" / "apps" / "48" / "split.png");
    CHECK_RESOLVES(r, "shadowed", opt(48), c / "apps" / "48" / "shadowed.png");

    // hicolor ends every chain, even one whose Inherits never names it.
    CHECK_RESOLVES(r, "hi", opt(48), b2 / "hicolor" / "48x48" / "apps" / "hi.png");
    CHECK_RESOLVES(r, "hisvg", opt(48), b2 / "hicolor" / "scalable" / "apps" / "hisvg.svg");

    // Unthemed: a base dir itself, then pixmaps; names with an extension.
    CHECK_RESOLVES(r, "pix", opt(48), pix / "pix.xpm");
    CHECK_RESOLVES(r, "unthemed", opt(48), b2 / "unthemed.png");
    CHECK_RESOLVES(r, "literal.png", opt(48), pix / "literal.png");
    CHECK_RESOLVES(r, "exact.png", opt(48), c / "apps" / "48" / "exact.png");
    CHECK_RESOLVES(r, "exact.svg", opt(32), c / "apps" / "32" / "exact.png");

    // Paths are taken as is.
    CHECK_RESOLVES(r, (root / "absolute" / "abs.png").string(), opt(48), root / "absolute" / "abs.png");
    TEST_CHECK(!r.resolve_icon((root / "absolute" / "missing.png").string(), opt(48)).has_value());

    // Missing names (the Child <-> Parent cycle must terminate), empty names.
    TEST_CHECK(!r.resolve_icon("no-such-icon", opt(48)).has_value());
    TEST_CHECK(!r.resolve_icon("", opt(48)).has_value());

    // Explicit theme, unknown theme (falls back to the default), index-less theme.
    CHECK_RESOLVES(r, "exact", opt(48, 1, "Other"), b2 / "Other" / "48x48" / "apps" / "exact.png");
    CHECK_RESOLVES(r, "exact", opt(48, 1, "NoSuchTheme"), c / "apps" / "48" / "exact.png");
    CHECK_RESOLVES(r, "g", opt(48, 1, "Guess"), b2 / "Guess" / "48x48" / "apps" / "g.png");
    CHECK_RESOLVES(r, "g", opt(16, 1, "Guess"), b2 / "Guess" / "16x16" / "apps" / "g.png");
    CHECK_RESOLVES(r, "g", opt(22, 1, "Guess"), b2 / "Guess" / "apps" / "22" / "g.png");
    CHECK_RESOLVES(r, "g", opt(200, 1, "Guess"), b2 / "Guess" / "scalable" / "apps" / "g.svg");

    // The size-only overload uses the default theme.
    auto legacy = r.resolve_icon("exact", 32u);
    TEST_CHECK(legacy.has_value());
    TEST_CHECK_EQ(*legacy, c / "apps" / "32" / "exact.png");

    // The cache: a new icon appears after clear_cache().
    touch(c / "apps" / "48" / "late.png");
    r.clear_cache();
    CHECK_RESOLVES(r, "late", opt(48), c / "apps" / "48" / "late.png");

    // ---- Configured theme ------------------------------------------------------------------
    {
        const fs::path cfg = root / "config";
        const fs::path home = root / "userhome";
        TEST_CHECK_EQ(configured_icon_theme(cfg, home, ""), "hicolor");
        TEST_CHECK_EQ(configured_icon_theme(cfg, home, "KDE"), "breeze");
        TEST_CHECK_EQ(configured_icon_theme(cfg, home, "ubuntu:GNOME"), "Adwaita");
        touch(home / ".gtkrc-2.0", "gtk-theme-name=\"X\"\ngtk-icon-theme-name = \"Gtk2Theme\"\n");
        TEST_CHECK_EQ(configured_icon_theme(cfg, home, ""), "Gtk2Theme");
        touch(cfg / "gtk-3.0" / "settings.ini", "[Settings]\ngtk-icon-theme-name=Gtk3Theme\n");
        TEST_CHECK_EQ(configured_icon_theme(cfg, home, ""), "Gtk3Theme");
        touch(cfg / "gtk-4.0" / "settings.ini", "[Settings]\ngtk-icon-theme-name=Gtk4Theme\n");
        TEST_CHECK_EQ(configured_icon_theme(cfg, home, "GNOME"), "Gtk4Theme");
        TEST_CHECK_EQ(configured_icon_theme(cfg, home, "KDE"), "Gtk4Theme");  // no kdeglobals yet
        touch(cfg / "kdeglobals", "[General]\nTheme=Wrong\n[Icons]\nTheme=KdeTheme\n");
        TEST_CHECK_EQ(configured_icon_theme(cfg, home, "KDE"), "KdeTheme");
        TEST_CHECK_EQ(configured_icon_theme(cfg, home, "GNOME"), "Gtk4Theme");  // GTK first off KDE
    }

    // ---- From the environment ---------------------------------------------------------------
    {
        const fs::path home = root / "envhome";
        touch(home / ".icons" / "dotfallback.png");
        touch(root / "envcfg" / "kdeglobals", "[Icons]\nTheme=Child\n");
        setenv("HOME", home.c_str(), 1);
        setenv("XDG_DATA_HOME", (root / "home").c_str(), 1);
        setenv("XDG_DATA_DIRS", (root / "data").c_str(), 1);
        setenv("XDG_CONFIG_HOME", (root / "envcfg").c_str(), 1);
        setenv("XDG_CURRENT_DESKTOP", "KDE", 1);

        LinuxIconResolver env_resolver;
        TEST_CHECK_EQ(env_resolver.default_theme(), "Child");
        const auto& dirs = env_resolver.base_dirs();
        TEST_CHECK_EQ(dirs.size(), 4u);
        TEST_CHECK_EQ(dirs[0], b1);
        TEST_CHECK_EQ(dirs[1], home / ".icons");
        TEST_CHECK_EQ(dirs[2], b2);
        TEST_CHECK_EQ(dirs[3], fs::path("/usr/share/pixmaps"));

        auto created = IconResolver::create();
        auto inherited = created->resolve_icon("inherited", 48u);
        TEST_CHECK(inherited.has_value());
        TEST_CHECK_EQ(*inherited, b2 / "Parent" / "64x64" / "apps" / "inherited.png");
        auto dot = created->resolve_icon("dotfallback", 48u);
        TEST_CHECK(dot.has_value());
        TEST_CHECK_EQ(*dot, home / ".icons" / "dotfallback.png");
    }

    for (size_t i = 0; i < std::size(env_names); ++i) restore_env(env_names[i], env_saved[i]);
    fs::remove_all(root, ec);

    // ---- The machine's own themes -----------------------------------------------------------
    // Only checked where the theme is installed: CI runners and servers have almost nothing.
    auto system = IconResolver::create();
    TEST_CHECK(system != nullptr);
    if (auto* lin = dynamic_cast<LinuxIconResolver*>(system.get())) {
        std::cout << "Configured icon theme: " << lin->default_theme() << std::endl;
    }
    for (const char* name : {"utilities-terminal", "konsole", "system-search", "text-editor",
                             "org.kde.konsole", "accessories-calculator"}) {
        auto icon_path = system->resolve_icon(name, 48u);
        std::cout << "Resolved icon '" << name << "' -> "
                  << (icon_path ? icon_path->string() : std::string("(none)")) << std::endl;
        if (icon_path) TEST_CHECK(fs::exists(*icon_path));
    }
    for (const char* theme : {"breeze", "Adwaita"}) {
        bool installed = false;
        for (const char* base : {"/usr/share/icons", "/usr/local/share/icons"}) {
            installed = installed || fs::exists(fs::path(base) / theme / "index.theme", ec);
        }
        if (!installed) continue;
        auto p = system->resolve_icon("utilities-terminal", opt(48, 1, theme));
        std::cout << "Resolved icon 'utilities-terminal' in " << theme << " -> "
                  << (p ? p->string() : std::string("(none)")) << std::endl;
        TEST_CHECK(p.has_value());
        TEST_CHECK(fs::exists(*p));
    }

    std::cout << "test_linux_icon passed!" << std::endl;
    return 0;
#endif
}
