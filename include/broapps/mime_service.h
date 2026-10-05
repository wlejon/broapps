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

// File associations: the default and registered handlers for a type, and open-with candidates.
// File types themselves come from brovfs (bro::vfs::MimeDatabase::system()); the type queries
// below are conveniences that forward there.
class MimeService {
public:
    virtual ~MimeService() = default;

    static std::unique_ptr<MimeService> create(std::shared_ptr<AppCatalog> catalog);

    virtual std::optional<AppInfo> get_default_app_for_mime(std::string_view mime_type) = 0;
    // The file is typed by name and content (a PNG saved as notes.txt opens in an image viewer);
    // a path that does not exist is typed by name.
    virtual std::optional<AppInfo> get_default_app_for_file(const std::filesystem::path& file_path) = 0;

    virtual std::vector<AppInfo> get_candidates_for_mime(std::string_view mime_type) = 0;
    virtual std::vector<AppInfo> get_candidates_for_file(const std::filesystem::path& file_path) = 0;

    // brovfs's answers: extension -> type ("application/octet-stream" when unknown), type ->
    // extensions with their dots, preferred first, and the type a file resolves to.
    virtual std::string extension_to_mime(std::string_view extension) const = 0;
    virtual std::vector<std::string> mime_to_extensions(std::string_view mime_type) const = 0;
    virtual std::string mime_for_file(const std::filesystem::path& file_path) const = 0;
};

}  // namespace broapps
