#include "mac_icon_resolver.h"

#if defined(__APPLE__)
#include <filesystem>

namespace broapps::mac_backend {

std::optional<std::filesystem::path> MacIconResolver::resolve_icon(
    const std::string& icon_name_or_path,
    uint32_t preferred_size) {

    (void)preferred_size;
    if (icon_name_or_path.empty()) return std::nullopt;

    std::error_code ec;
    std::filesystem::path p(icon_name_or_path);
    if (std::filesystem::exists(p, ec) && std::filesystem::is_regular_file(p, ec)) {
        return p;
    }

    // Try in System CoreTypes
    std::string icon_filename = icon_name_or_path;
    if (!icon_filename.ends_with(".icns")) {
        icon_filename += ".icns";
    }

    auto core_types = std::filesystem::path("/System/Library/CoreServices/CoreTypes.bundle/Contents/Resources") / icon_filename;
    if (std::filesystem::exists(core_types, ec)) {
        return core_types;
    }

    return std::nullopt;
}

}  // namespace broapps::mac_backend

namespace broapps {

std::unique_ptr<IconResolver> IconResolver::create() {
    return std::make_unique<mac_backend::MacIconResolver>();
}

}  // namespace broapps
#endif
