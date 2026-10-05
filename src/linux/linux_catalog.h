#pragma once

#include "catalog_base.h"

namespace broapps::linux_backend {

class LinuxCatalog : public CatalogBase {
public:
    explicit LinuxCatalog(CatalogConfig config);
    ~LinuxCatalog() override = default;

    void refresh() override;
};

}  // namespace broapps::linux_backend
