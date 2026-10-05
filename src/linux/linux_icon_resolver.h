#pragma once

#include "broapps/icon_resolver.h"
#include <string>
#include <vector>

namespace broapps::linux_backend {

class LinuxIconResolver : public IconResolver {
public:
    LinuxIconResolver();
    ~LinuxIconResolver() override = default;

    std::optional<std::filesystem::path> resolve_icon(
        const std::string& icon_name_or_path,
        uint32_t preferred_size = 48) override;

private:
    std::vector<std::string> icon_search_dirs_;
};

}  // namespace broapps::linux_backend
