#pragma once

#if defined(__APPLE__)
#include "src/common/mime_service_base.h"

namespace broapps::mac_backend {

class MacMimeService : public MimeServiceBase {
public:
    explicit MacMimeService(std::shared_ptr<AppCatalog> catalog);
    ~MacMimeService() override = default;

    bool set_default_app_for_mime(std::string_view mime_type, std::string_view app_id) override;
    std::optional<AppInfo> get_default_app_for_mime(std::string_view mime_type) override;
    std::vector<AppInfo> get_candidates_for_mime(std::string_view mime_type) override;
};

}  // namespace broapps::mac_backend
#endif
