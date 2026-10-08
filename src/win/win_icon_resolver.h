#pragma once

#ifdef _WIN32
#include "broapps/icon_resolver.h"
#include <string>

namespace broapps::win_backend {

class WinIconResolver : public IconResolver {
public:
    WinIconResolver() = default;
    ~WinIconResolver() override = default;

    using IconResolver::resolve_icon;
    std::optional<std::filesystem::path> resolve_icon(
        const std::string& icon_name_or_path,
        uint32_t preferred_size = 48) override;
};

}  // namespace broapps::win_backend
#endif
