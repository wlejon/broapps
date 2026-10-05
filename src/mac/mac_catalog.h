#pragma once

#if defined(__APPLE__)
#include "catalog_base.h"

namespace broapps::mac_backend {

class MacCatalog : public CatalogBase {
public:
    explicit MacCatalog(CatalogConfig config);
    ~MacCatalog() override = default;

    void refresh() override;
};

}  // namespace broapps::mac_backend
#endif
