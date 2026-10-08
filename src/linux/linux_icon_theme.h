#pragma once

// freedesktop Icon Theme Specification pieces: index.theme parsing, the
// per-directory size matching rules, and which theme the desktop is configured
// to use. The lookup itself (FindIcon / LookupIcon) is in linux_icon_resolver.cpp.

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace broapps::linux_backend {

enum class IconDirType { Fixed, Scalable, Threshold };

// One entry of an index.theme Directories= / ScaledDirectories= list.
struct IconThemeDir {
    std::string subdir;  // relative to the theme root, e.g. "apps/48" or "48x48@2/apps"
    IconDirType type = IconDirType::Threshold;
    int size = 0;
    int scale = 1;
    int min_size = 0;
    int max_size = 0;
    int threshold = 2;

    // The file names present in this subdirectory, one set per theme root that has it
    // (in base-dir order), read once when the theme loads.
    struct Listing {
        std::filesystem::path dir;
        std::unordered_set<std::string> files;
    };
    std::vector<Listing> listings;
};

struct IconTheme {
    std::string name;
    std::vector<std::filesystem::path> roots;  // <basedir>/<name> for every basedir that has it
    std::vector<std::string> parents;          // Inherits=, in order
    std::vector<IconThemeDir> dirs;
    bool has_index = false;
};

// DirectoryMatchesSize / DirectorySizeDistance from the spec (with the spec's
// Threshold typos corrected: the distance is measured to Size±Threshold).
bool icon_dir_matches_size(const IconThemeDir& dir, int size, int scale);
int icon_dir_size_distance(const IconThemeDir& dir, int size, int scale);

// Parse the text of an index.theme into `theme` (parents, dirs). Directories named
// in Directories= and ScaledDirectories= without a section are skipped, as are
// sections without a Size=.
void parse_index_theme(std::string_view text, IconTheme& theme);

// Load `name` from the base dirs: the first index.theme found wins; every base dir
// holding the theme contributes its directory listings. A theme found without any
// index.theme gets its directories guessed from the tree ("48x48/apps", "apps/48",
// "scalable/apps", "48x48@2/apps"). Returns nullptr when no base dir has the theme.
std::unique_ptr<IconTheme> load_icon_theme(const std::string& name,
                                           const std::vector<std::filesystem::path>& base_dirs);

// The configured icon theme: KDE's kdeglobals [Icons] Theme=, GTK 4/3 settings.ini
// gtk-icon-theme-name, ~/.gtkrc-2.0 (KDE first on a KDE session, GTK first otherwise),
// then the desktop's stock theme (breeze on KDE, Adwaita on GNOME), else "hicolor".
// `config_home` is $XDG_CONFIG_HOME (or ~/.config), `home` is $HOME (may be empty).
std::string configured_icon_theme(const std::filesystem::path& config_home,
                                  const std::filesystem::path& home,
                                  const std::string& current_desktop);

}  // namespace broapps::linux_backend
