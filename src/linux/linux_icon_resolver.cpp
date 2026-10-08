#include "linux_icon_resolver.h"

#include <climits>
#include <cstdlib>

namespace broapps::linux_backend {

namespace fs = std::filesystem;

namespace {

// Spec order: PNG, SVG, XPM.
constexpr const char* kExtensions[] = {".png", ".svg", ".xpm"};

constexpr auto kRevalidateInterval = std::chrono::seconds(2);

std::vector<std::string> split_colon(std::string_view str) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= str.size()) {
        size_t end = str.find(':', start);
        if (end == std::string_view::npos) end = str.size();
        if (end > start) out.emplace_back(str.substr(start, end - start));
        start = end + 1;
    }
    return out;
}

std::string env_or_empty(const char* name) {
    const char* v = std::getenv(name);
    return v ? std::string(v) : std::string();
}

bool has_image_extension(const std::string& name) {
    for (const char* ext : kExtensions) {
        if (name.size() > std::char_traits<char>::length(ext) && name.ends_with(ext)) return true;
    }
    return false;
}

fs::file_time_type mtime_or_min(const fs::path& p) {
    std::error_code ec;
    auto t = fs::last_write_time(p, ec);
    return ec ? fs::file_time_type::min() : t;
}

}  // namespace

LinuxIconResolver::LinuxIconResolver() : from_environment_(true) {
    const std::string home = env_or_empty("HOME");
    const std::string data_home = env_or_empty("XDG_DATA_HOME");
    const std::string config_home = env_or_empty("XDG_CONFIG_HOME");
    const std::string data_dirs = env_or_empty("XDG_DATA_DIRS");

    home_ = home;
    if (!data_home.empty()) {
        base_dirs_.push_back(fs::path(data_home) / "icons");
    } else if (!home.empty()) {
        base_dirs_.push_back(fs::path(home) / ".local" / "share" / "icons");
    }
    if (!home.empty()) base_dirs_.push_back(fs::path(home) / ".icons");

    auto dirs = split_colon(data_dirs);
    if (dirs.empty()) dirs = {"/usr/local/share", "/usr/share"};
    for (const auto& d : dirs) base_dirs_.push_back(fs::path(d) / "icons");
    base_dirs_.push_back("/usr/share/pixmaps");

    if (!config_home.empty()) config_home_ = config_home;
    else if (!home.empty()) config_home_ = fs::path(home) / ".config";
    current_desktop_ = env_or_empty("XDG_CURRENT_DESKTOP");
    if (current_desktop_.empty()) current_desktop_ = env_or_empty("DESKTOP_SESSION");

    std::lock_guard lock(mu_);
    reset_locked();
}

LinuxIconResolver::LinuxIconResolver(std::vector<fs::path> base_dirs, std::string default_theme)
    : base_dirs_(std::move(base_dirs)), default_theme_(std::move(default_theme)) {
    if (default_theme_.empty()) default_theme_ = "hicolor";
    std::lock_guard lock(mu_);
    reset_locked();
}

void LinuxIconResolver::clear_cache() {
    std::lock_guard lock(mu_);
    reset_locked();
}

std::string LinuxIconResolver::default_theme() {
    std::lock_guard lock(mu_);
    revalidate_locked();
    return default_theme_;
}

void LinuxIconResolver::watch_locked(const fs::path& p) {
    watched_.emplace(p, mtime_or_min(p));
}

void LinuxIconResolver::reset_locked() {
    themes_.clear();
    watched_.clear();
    for (const auto& b : base_dirs_) watch_locked(b);
    if (from_environment_) {
        if (!config_home_.empty()) {
            watch_locked(config_home_ / "kdeglobals");
            watch_locked(config_home_ / "gtk-4.0" / "settings.ini");
            watch_locked(config_home_ / "gtk-3.0" / "settings.ini");
        }
        if (!home_.empty()) watch_locked(home_ / ".gtkrc-2.0");
        default_theme_ = configured_icon_theme(config_home_, home_, current_desktop_);
    }
    last_check_ = std::chrono::steady_clock::now();
}

void LinuxIconResolver::revalidate_locked() {
    auto now = std::chrono::steady_clock::now();
    if (now - last_check_ < kRevalidateInterval) return;
    last_check_ = now;
    for (const auto& [path, mtime] : watched_) {
        if (mtime_or_min(path) != mtime) {
            reset_locked();
            return;
        }
    }
}

const IconTheme* LinuxIconResolver::theme_named(const std::string& name) {
    auto it = themes_.find(name);
    if (it != themes_.end()) return it->second.get();
    auto theme = load_icon_theme(name, base_dirs_);
    if (theme) {
        for (const auto& root : theme->roots) watch_locked(root);
    }
    const IconTheme* raw = theme.get();
    themes_.emplace(name, std::move(theme));
    return raw;
}

std::optional<fs::path> LinuxIconResolver::resolve_icon(const std::string& icon_name_or_path,
                                                        uint32_t preferred_size) {
    IconLookupOptions options;
    options.size = preferred_size;
    return resolve_icon(icon_name_or_path, options);
}

std::optional<fs::path> LinuxIconResolver::resolve_icon(const std::string& icon_name_or_path,
                                                        const IconLookupOptions& options) {
    if (icon_name_or_path.empty()) return std::nullopt;

    // A path (Icon=/opt/app/icon.png) is taken as is, never looked up as a name.
    if (icon_name_or_path.find('/') != std::string::npos) {
        std::error_code ec;
        fs::path p(icon_name_or_path);
        if (fs::is_regular_file(p, ec)) return p;
        return std::nullopt;
    }

    Request req;
    req.size = options.size > 0 ? static_cast<int>(std::min<uint32_t>(options.size, 1u << 16)) : 48;
    req.scale = options.scale > 0 ? static_cast<int>(std::min<uint32_t>(options.scale, 16u)) : 1;
    if (has_image_extension(icon_name_or_path)) {
        // Icon=foo.png is not spec-conforming but common: look up "foo", and accept the
        // literal file in the fallback directories.
        req.exact = icon_name_or_path;
        req.name = fs::path(icon_name_or_path).stem().string();
    } else {
        req.name = icon_name_or_path;
    }

    std::lock_guard lock(mu_);
    revalidate_locked();

    std::string theme = default_theme_;
    if (!options.theme.empty() && theme_named(options.theme)) theme = options.theme;
    return find_icon(req, theme);
}

std::optional<fs::path> LinuxIconResolver::find_icon(const Request& req, const std::string& theme) {
    std::unordered_set<std::string> visited;
    if (auto r = find_in_theme(req, theme, visited)) return r;
    if (auto r = find_in_theme(req, "hicolor", visited)) return r;
    return lookup_fallback(req);
}

std::optional<fs::path> LinuxIconResolver::find_in_theme(const Request& req, const std::string& theme_name,
                                                         std::unordered_set<std::string>& visited) {
    // `visited` is the cycle guard (A inherits B inherits A) and also keeps a theme
    // reached twice through a diamond from being searched twice.
    if (!visited.insert(theme_name).second) return std::nullopt;
    const IconTheme* theme = theme_named(theme_name);
    if (!theme) return std::nullopt;
    if (auto r = lookup_in(*theme, req)) return r;
    for (const auto& parent : theme->parents) {
        if (auto r = find_in_theme(req, parent, visited)) return r;
    }
    return std::nullopt;
}

std::optional<fs::path> LinuxIconResolver::lookup_in(const IconTheme& theme, const Request& req) const {
    std::string candidates[std::size(kExtensions)];
    for (size_t i = 0; i < std::size(kExtensions); ++i) candidates[i] = req.name + kExtensions[i];

    // Pass 1: the first directory whose size matches exactly.
    for (const auto& dir : theme.dirs) {
        if (!icon_dir_matches_size(dir, req.size, req.scale)) continue;
        for (const auto& listing : dir.listings) {
            for (const auto& c : candidates) {
                if (listing.files.count(c)) return listing.dir / c;
            }
        }
    }

    // Pass 2: the closest size anywhere in the theme (first one on a tie).
    std::optional<fs::path> closest;
    int best = INT_MAX;
    for (const auto& dir : theme.dirs) {
        if (dir.listings.empty()) continue;
        const int distance = icon_dir_size_distance(dir, req.size, req.scale);
        if (distance >= best) continue;
        for (const auto& listing : dir.listings) {
            bool found = false;
            for (const auto& c : candidates) {
                if (listing.files.count(c)) {
                    closest = listing.dir / c;
                    best = distance;
                    found = true;
                    break;
                }
            }
            if (found) break;
        }
    }
    return closest;
}

std::optional<fs::path> LinuxIconResolver::lookup_fallback(const Request& req) const {
    for (const auto& base : base_dirs_) {
        std::error_code ec;
        if (!req.exact.empty()) {
            fs::path p = base / req.exact;
            if (fs::is_regular_file(p, ec)) return p;
        }
        for (const char* ext : kExtensions) {
            fs::path p = base / (req.name + ext);
            if (fs::is_regular_file(p, ec)) return p;
        }
    }
    return std::nullopt;
}

}  // namespace broapps::linux_backend

#if defined(__linux__)
namespace broapps {

std::unique_ptr<IconResolver> IconResolver::create() {
    return std::make_unique<linux_backend::LinuxIconResolver>();
}

}  // namespace broapps
#endif
