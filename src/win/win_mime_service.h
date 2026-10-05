#pragma once

#ifdef _WIN32
#include "src/common/mime_service_base.h"

namespace broapps::win_backend {

class WinMimeService : public MimeServiceBase {
public:
    explicit WinMimeService(std::shared_ptr<AppCatalog> catalog);
    ~WinMimeService() override = default;

    std::optional<AppInfo> get_default_app_for_mime(std::string_view mime_type) override;
    std::vector<AppInfo> get_candidates_for_mime(std::string_view mime_type) override;

    std::string extension_to_mime(std::string_view extension) const override;
    std::vector<std::string> mime_to_extensions(std::string_view mime_type) const override;
};

}  // namespace broapps::win_backend
#endif
