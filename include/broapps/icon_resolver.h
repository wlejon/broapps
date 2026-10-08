#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace broapps {

// What to look an icon up for. `theme` empty means the desktop's configured theme
// (on Linux: KDE kdeglobals, then GTK settings.ini, else hicolor); `scale` is the
// integer UI scale the icon is drawn at (2 on a 2x display), so a 48@2 request
// prefers a 48x48@2 directory over a 96x96 one.
struct IconLookupOptions {
    uint32_t size = 48;
    uint32_t scale = 1;
    std::string theme;
};

class IconResolver {
public:
    virtual ~IconResolver() = default;

    static std::unique_ptr<IconResolver> create();

    virtual std::optional<std::filesystem::path> resolve_icon(
        const std::string& icon_name_or_path,
        uint32_t preferred_size = 48) = 0;

    // Full lookup. Backends without themes (Windows, macOS) answer it with the
    // size-only overload.
    virtual std::optional<std::filesystem::path> resolve_icon(
        const std::string& icon_name_or_path,
        const IconLookupOptions& options) {
        return resolve_icon(icon_name_or_path, options.size);
    }
};

}  // namespace broapps
