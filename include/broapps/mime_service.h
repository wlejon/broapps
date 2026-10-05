#pragma once

#include "broapps/app_catalog.h"
#include "broapps/app_info.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace broapps {

class MimeService {
public:
    virtual ~MimeService() = default;

    static std::unique_ptr<MimeService> create(std::shared_ptr<AppCatalog> catalog);

    virtual std::optional<AppInfo> get_default_app_for_mime(std::string_view mime_type) = 0;
    virtual std::optional<AppInfo> get_default_app_for_file(const std::filesystem::path& file_path) = 0;

    virtual std::vector<AppInfo> get_candidates_for_mime(std::string_view mime_type) = 0;
    virtual std::vector<AppInfo> get_candidates_for_file(const std::filesystem::path& file_path) = 0;

    virtual std::string extension_to_mime(std::string_view extension) const = 0;
    virtual std::vector<std::string> mime_to_extensions(std::string_view mime_type) const = 0;
};

}  // namespace broapps
