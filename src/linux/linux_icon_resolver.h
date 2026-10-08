#pragma once

#include "broapps/icon_resolver.h"
#include "linux_icon_theme.h"

#include <chrono>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace broapps::linux_backend {

// freedesktop Icon Theme Specification lookup: the requested (or configured)
// theme, its Inherits chain, hicolor, then the unthemed fallback directories.
// Parsed themes and their directory listings are cached; the cache drops itself
// when a base dir, a loaded theme's root, or the desktop's theme setting changes
// (checked at most every couple of seconds), and clear_cache() drops it now.
class LinuxIconResolver : public IconResolver {
public:
    // Base dirs from the environment, in lookup order: $XDG_DATA_HOME/icons
    // (~/.local/share/icons), ~/.icons, $XDG_DATA_DIRS/icons (/usr/local/share,
    // /usr/share), /usr/share/pixmaps. Theme from configured_icon_theme().
    LinuxIconResolver();

    // Explicit base dirs and default theme (empty: "hicolor"), with no environment or
    // desktop-settings reads; for tests and embedders with their own icon roots.
    LinuxIconResolver(std::vector<std::filesystem::path> base_dirs, std::string default_theme);

    ~LinuxIconResolver() override = default;

    std::optional<std::filesystem::path> resolve_icon(
        const std::string& icon_name_or_path,
        uint32_t preferred_size = 48) override;

    std::optional<std::filesystem::path> resolve_icon(
        const std::string& icon_name_or_path,
        const IconLookupOptions& options) override;

    void clear_cache();

    const std::vector<std::filesystem::path>& base_dirs() const { return base_dirs_; }
    std::string default_theme();

private:
    struct Request {
        std::string name;   // icon name without extension
        std::string exact;  // the name as given, when it carried an image extension
        int size = 48;
        int scale = 1;
    };

    std::optional<std::filesystem::path> find_icon(const Request& req, const std::string& theme);
    std::optional<std::filesystem::path> find_in_theme(const Request& req, const std::string& theme,
                                                       std::unordered_set<std::string>& visited);
    std::optional<std::filesystem::path> lookup_in(const IconTheme& theme, const Request& req) const;
    std::optional<std::filesystem::path> lookup_fallback(const Request& req) const;

    const IconTheme* theme_named(const std::string& name);
    void revalidate_locked();
    void reset_locked();
    void watch_locked(const std::filesystem::path& p);

    std::vector<std::filesystem::path> base_dirs_;
    bool from_environment_ = false;
    std::filesystem::path config_home_;
    std::filesystem::path home_;
    std::string current_desktop_;

    std::mutex mu_;
    std::string default_theme_;
    std::unordered_map<std::string, std::unique_ptr<IconTheme>> themes_;  // null = not installed
    std::map<std::filesystem::path, std::filesystem::file_time_type> watched_;
    std::chrono::steady_clock::time_point last_check_{};
};

}  // namespace broapps::linux_backend
