#pragma once

#if defined(__APPLE__)
#include "broapps/icon_resolver.h"

namespace broapps::mac_backend {

class MacIconResolver : public IconResolver {
public:
    MacIconResolver() = default;
    ~MacIconResolver() override = default;

    using IconResolver::resolve_icon;
    std::optional<std::filesystem::path> resolve_icon(
        const std::string& icon_name_or_path,
        uint32_t preferred_size = 48) override;
};

}  // namespace broapps::mac_backend
#endif
