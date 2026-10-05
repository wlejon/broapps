#pragma once

#include "src/common/mime_service_base.h"
#include "mimeapps_parser.h"

namespace broapps::linux_backend {

class LinuxMimeService : public MimeServiceBase {
public:
    explicit LinuxMimeService(std::shared_ptr<AppCatalog> catalog);
    ~LinuxMimeService() override = default;

    std::optional<AppInfo> get_default_app_for_mime(std::string_view mime_type) override;
    std::vector<AppInfo> get_candidates_for_mime(std::string_view mime_type) override;

private:
    MimeAssociations assocs_;
};

}  // namespace broapps::linux_backend
