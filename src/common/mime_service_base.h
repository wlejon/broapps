#pragma once

#include "broapps/mime_service.h"

#include <memory>
#include <string>
#include <vector>

namespace broapps {

// Associations only: what a file *is* comes from brovfs (bro::vfs::MimeDatabase::system(),
// the platform's type database), and the backends here answer which applications open a type.
class MimeServiceBase : public MimeService {
public:
    explicit MimeServiceBase(std::shared_ptr<AppCatalog> catalog);
    ~MimeServiceBase() override = default;

    std::optional<AppInfo> get_default_app_for_file(const std::filesystem::path& file_path) override;
    std::vector<AppInfo> get_candidates_for_file(const std::filesystem::path& file_path) override;

    std::string extension_to_mime(std::string_view extension) const override;
    std::vector<std::string> mime_to_extensions(std::string_view mime_type) const override;
    std::string mime_for_file(const std::filesystem::path& file_path) const override;

protected:
    std::shared_ptr<AppCatalog> catalog_;
    std::optional<AppInfo> find_app_in_catalog(std::string_view id_or_path);
    std::vector<AppInfo> filter_catalog_by_mime(std::string_view mime_type);
};

}  // namespace broapps
