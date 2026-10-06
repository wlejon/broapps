#pragma once

#ifdef _WIN32
#include "src/common/mime_service_base.h"

namespace broapps::win_backend {

class WinMimeService : public MimeServiceBase {
public:
    explicit WinMimeService(std::shared_ptr<AppCatalog> catalog);
    ~WinMimeService() override = default;

    bool set_default_app_for_mime(std::string_view mime_type, std::string_view app_id) override;
    std::optional<AppInfo> get_default_app_for_mime(std::string_view mime_type) override;
    std::vector<AppInfo> get_candidates_for_mime(std::string_view mime_type) override;

private:
    std::optional<AppInfo> registered_default(std::string_view mime_type) const;
};

}  // namespace broapps::win_backend
#endif
