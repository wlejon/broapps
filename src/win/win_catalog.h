#pragma once

#ifdef _WIN32
#include "catalog_base.h"

namespace broapps::win_backend {

class WinCatalog : public CatalogBase {
public:
    explicit WinCatalog(CatalogConfig config);
    ~WinCatalog() override = default;

    void refresh() override;
};

}  // namespace broapps::win_backend
#endif
