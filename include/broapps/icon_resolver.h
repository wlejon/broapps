#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace broapps {

class IconResolver {
public:
    virtual ~IconResolver() = default;

    static std::unique_ptr<IconResolver> create();

    virtual std::optional<std::filesystem::path> resolve_icon(
        const std::string& icon_name_or_path,
        uint32_t preferred_size = 48) = 0;
};

}  // namespace broapps
