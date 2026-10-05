#include "mime_service_base.h"

#include <brovfs/mime.h>

namespace broapps {

MimeServiceBase::MimeServiceBase(std::shared_ptr<AppCatalog> catalog)
    : catalog_(std::move(catalog)) {}

std::optional<AppInfo> MimeServiceBase::get_default_app_for_file(const std::filesystem::path& file_path) {
    return get_default_app_for_mime(mime_for_file(file_path));
}

std::vector<AppInfo> MimeServiceBase::get_candidates_for_file(const std::filesystem::path& file_path) {
    return get_candidates_for_mime(mime_for_file(file_path));
}

std::string MimeServiceBase::mime_for_file(const std::filesystem::path& file_path) const {
    return bro::vfs::MimeDatabase::system().type_for_file(file_path).mime;
}

std::string MimeServiceBase::extension_to_mime(std::string_view extension) const {
    return bro::vfs::MimeDatabase::system().type_for_extension(extension);
}

std::vector<std::string> MimeServiceBase::mime_to_extensions(std::string_view mime_type) const {
    std::vector<std::string> out;
    for (auto& ext : bro::vfs::MimeDatabase::system().extensions_for_type(mime_type)) {
        out.push_back("." + ext);
    }
    return out;
}

std::optional<AppInfo> MimeServiceBase::find_app_in_catalog(std::string_view id_or_path) {
    if (!catalog_) return std::nullopt;

    // 1. Direct ID match
    auto app = catalog_->find_by_id(id_or_path);
    if (app) return app;

    // 2. Scan for matching executable or desktop file
    for (const auto& a : catalog_->apps()) {
        if (a.executable_path == id_or_path ||
            a.desktop_file_path == id_or_path ||
            a.bundle_id == id_or_path ||
            a.aumid == id_or_path ||
            a.shortcut_path == id_or_path) {
            return a;
        }
    }

    return std::nullopt;
}

std::vector<AppInfo> MimeServiceBase::filter_catalog_by_mime(std::string_view mime_type) {
    if (!catalog_) return {};
    return catalog_->find_by_mime_type(mime_type);
}

}  // namespace broapps
