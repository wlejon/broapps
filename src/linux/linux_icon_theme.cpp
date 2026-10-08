#include "linux_icon_theme.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <sstream>
#include <unordered_map>

namespace broapps::linux_backend {

namespace fs = std::filesystem;

namespace {

std::string_view trim(std::string_view sv) {
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front()))) sv.remove_prefix(1);
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back()))) sv.remove_suffix(1);
    return sv;
}

std::string lower(std::string_view sv) {
    std::string out(sv);
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

std::vector<std::string> split_list(std::string_view sv, char sep) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= sv.size()) {
        size_t end = sv.find(sep, start);
        if (end == std::string_view::npos) end = sv.size();
        std::string_view item = trim(sv.substr(start, end - start));
        if (!item.empty()) out.emplace_back(item);
        start = end + 1;
    }
    return out;
}

bool parse_int(std::string_view sv, int& out) {
    sv = trim(sv);
    if (sv.empty()) return false;
    int v = 0;
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), v);
    if (ec != std::errc() || ptr != sv.data() + sv.size()) return false;
    out = v;
    return true;
}

// Calls fn(section, key, value) for each `key=value` line of an INI / desktop-entry file.
void for_each_ini_entry(std::string_view text,
                        const std::function<void(std::string_view, std::string_view, std::string_view)>& fn) {
    std::string_view section;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t eol = text.find('\n', pos);
        if (eol == std::string_view::npos) eol = text.size();
        std::string_view line = trim(text.substr(pos, eol - pos));
        pos = eol + 1;
        if (line.empty() || line.front() == '#' || line.front() == ';') continue;
        if (line.front() == '[') {
            size_t close = line.find(']');
            section = close == std::string_view::npos ? line.substr(1) : line.substr(1, close - 1);
            continue;
        }
        size_t eq = line.find('=');
        if (eq == std::string_view::npos) continue;
        fn(section, trim(line.substr(0, eq)), trim(line.substr(eq + 1)));
    }
}

bool read_file(const fs::path& p, std::string& out) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

// "48x48", "48x48@2", "48", "48@2x", "48@2" -> size / scale.
bool parse_size_component(std::string_view c, int& size, int& scale) {
    int s = 1;
    size_t at = c.find('@');
    if (at != std::string_view::npos) {
        std::string_view sc = c.substr(at + 1);
        if (!sc.empty() && (sc.back() == 'x' || sc.back() == 'X')) sc.remove_suffix(1);
        if (!parse_int(sc, s) || s < 1) return false;
        c = c.substr(0, at);
    }
    size_t x = c.find('x');
    int w = 0;
    if (x != std::string_view::npos) {
        int h = 0;
        if (!parse_int(c.substr(0, x), w) || !parse_int(c.substr(x + 1), h) || w != h) return false;
    } else if (!parse_int(c, w)) {
        return false;
    }
    if (w <= 0) return false;
    size = w;
    scale = s;
    return true;
}

// A theme dir without an index.theme: guess each icon directory's size from its path.
void guess_theme_dirs(IconTheme& theme) {
    std::vector<std::string> subdirs;
    for (const auto& root : theme.roots) {
        std::error_code ec;
        fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
        for (; !ec && it != end; it.increment(ec)) {
            if (it.depth() > 2) { it.disable_recursion_pending(); continue; }
            std::error_code dec;
            if (!it->is_directory(dec)) continue;
            subdirs.push_back(fs::relative(it->path(), root, dec).generic_string());
        }
    }
    std::sort(subdirs.begin(), subdirs.end());
    subdirs.erase(std::unique(subdirs.begin(), subdirs.end()), subdirs.end());

    for (const auto& sub : subdirs) {
        IconThemeDir d;
        d.subdir = sub;
        bool sized = false;
        for (const auto& comp : split_list(sub, '/')) {
            if (comp == "scalable") {
                d.type = IconDirType::Scalable;
                d.size = 48;
                d.min_size = 8;
                d.max_size = 512;
                sized = true;
            } else if (parse_size_component(comp, d.size, d.scale)) {
                d.type = IconDirType::Threshold;
                sized = true;
            }
        }
        if (sized) theme.dirs.push_back(std::move(d));
    }
}

}  // namespace

bool icon_dir_matches_size(const IconThemeDir& dir, int size, int scale) {
    if (dir.scale != scale) return false;
    switch (dir.type) {
        case IconDirType::Fixed: return dir.size == size;
        case IconDirType::Scalable: return dir.min_size <= size && size <= dir.max_size;
        case IconDirType::Threshold:
            return dir.size - dir.threshold <= size && size <= dir.size + dir.threshold;
    }
    return false;
}

int icon_dir_size_distance(const IconThemeDir& dir, int size, int scale) {
    const int want = size * scale;
    switch (dir.type) {
        case IconDirType::Fixed: return std::abs(dir.size * dir.scale - want);
        case IconDirType::Scalable:
            if (want < dir.min_size * dir.scale) return dir.min_size * dir.scale - want;
            if (want > dir.max_size * dir.scale) return want - dir.max_size * dir.scale;
            return 0;
        case IconDirType::Threshold: {
            const int lo = (dir.size - dir.threshold) * dir.scale;
            const int hi = (dir.size + dir.threshold) * dir.scale;
            if (want < lo) return lo - want;
            if (want > hi) return want - hi;
            return 0;
        }
    }
    return 0;
}

void parse_index_theme(std::string_view text, IconTheme& theme) {
    std::vector<std::string> dir_names;
    struct Raw {
        std::string type;
        int size = 0, scale = 1, min_size = -1, max_size = -1, threshold = 2;
        bool has_size = false;
    };
    std::unordered_map<std::string, Raw> raw;

    for_each_ini_entry(text, [&](std::string_view section, std::string_view key, std::string_view value) {
        if (section == "Icon Theme") {
            if (key == "Inherits") {
                theme.parents = split_list(value, ',');
            } else if (key == "Directories" || key == "ScaledDirectories") {
                for (auto& d : split_list(value, ',')) dir_names.push_back(std::move(d));
            }
            return;
        }
        Raw& r = raw[std::string(section)];
        int v = 0;
        if (key == "Size" && parse_int(value, v)) { r.size = v; r.has_size = true; }
        else if (key == "Scale" && parse_int(value, v) && v > 0) r.scale = v;
        else if (key == "MinSize" && parse_int(value, v)) r.min_size = v;
        else if (key == "MaxSize" && parse_int(value, v)) r.max_size = v;
        else if (key == "Threshold" && parse_int(value, v)) r.threshold = v;
        else if (key == "Type") r.type = lower(value);
    });

    std::unordered_set<std::string> seen;
    for (const auto& name : dir_names) {
        if (!seen.insert(name).second) continue;
        auto it = raw.find(name);
        if (it == raw.end() || !it->second.has_size) continue;
        const Raw& r = it->second;
        IconThemeDir d;
        d.subdir = name;
        d.size = r.size;
        d.scale = r.scale;
        d.threshold = r.threshold;
        d.min_size = r.min_size >= 0 ? r.min_size : r.size;
        d.max_size = r.max_size >= 0 ? r.max_size : r.size;
        if (r.type == "fixed") d.type = IconDirType::Fixed;
        else if (r.type == "scalable") d.type = IconDirType::Scalable;
        else d.type = IconDirType::Threshold;
        theme.dirs.push_back(std::move(d));
    }
}

std::unique_ptr<IconTheme> load_icon_theme(const std::string& name,
                                           const std::vector<fs::path>& base_dirs) {
    if (name.empty() || name.find('/') != std::string::npos || name == "." || name == "..") return nullptr;

    auto theme = std::make_unique<IconTheme>();
    theme->name = name;
    for (const auto& base : base_dirs) {
        std::error_code ec;
        fs::path root = base / name;
        if (!fs::is_directory(root, ec)) continue;
        theme->roots.push_back(root);
        if (!theme->has_index) {
            std::string text;
            if (read_file(root / "index.theme", text)) {
                parse_index_theme(text, *theme);
                theme->has_index = true;
            }
        }
    }
    if (theme->roots.empty()) return nullptr;
    if (!theme->has_index) guess_theme_dirs(*theme);

    for (auto& dir : theme->dirs) {
        for (const auto& root : theme->roots) {
            std::error_code ec;
            fs::path p = root / dir.subdir;
            fs::directory_iterator it(p, fs::directory_options::skip_permission_denied, ec), end;
            if (ec) continue;
            IconThemeDir::Listing listing;
            listing.dir = p;
            for (; !ec && it != end; it.increment(ec)) {
                listing.files.insert(it->path().filename().string());
            }
            if (!listing.files.empty()) dir.listings.push_back(std::move(listing));
        }
    }
    return theme;
}

std::string configured_icon_theme(const fs::path& config_home,
                                  const fs::path& home,
                                  const std::string& current_desktop) {
    auto from_kde = [&]() -> std::string {
        std::string text, theme;
        if (!read_file(config_home / "kdeglobals", text)) return {};
        for_each_ini_entry(text, [&](std::string_view section, std::string_view key, std::string_view value) {
            if (section == "Icons" && key == "Theme") theme = std::string(value);
        });
        return theme;
    };
    auto from_gtk = [&]() -> std::string {
        for (const char* sub : {"gtk-4.0", "gtk-3.0"}) {
            std::string text, theme;
            if (!read_file(config_home / sub / "settings.ini", text)) continue;
            for_each_ini_entry(text, [&](std::string_view section, std::string_view key, std::string_view value) {
                if (section == "Settings" && key == "gtk-icon-theme-name") theme = std::string(value);
            });
            if (!theme.empty()) return theme;
        }
        // GTK 2: gtk-icon-theme-name = "Name" (quoted, no sections).
        std::string text, theme;
        if (!home.empty() && read_file(home / ".gtkrc-2.0", text)) {
            for_each_ini_entry(text, [&](std::string_view, std::string_view key, std::string_view value) {
                if (key != "gtk-icon-theme-name") return;
                if (value.size() >= 2 && (value.front() == '"' || value.front() == '\'') &&
                    value.back() == value.front()) {
                    value = value.substr(1, value.size() - 2);
                }
                theme = std::string(value);
            });
        }
        return theme;
    };

    const std::string desktop = lower(current_desktop);
    const bool kde = desktop.find("kde") != std::string::npos;
    const bool gnome = desktop.find("gnome") != std::string::npos;

    std::string theme = kde ? from_kde() : from_gtk();
    if (theme.empty()) theme = kde ? from_gtk() : from_kde();
    if (theme.empty() && kde) theme = "breeze";
    if (theme.empty() && gnome) theme = "Adwaita";
    if (theme.empty()) theme = "hicolor";
    return theme;
}

}  // namespace broapps::linux_backend
