#include "broapps/app_catalog.h"
#include "broapps/catalog_watcher.h"
#include "tests/test_common.h"

#include <iostream>
#include <thread>

int main() {
#ifndef __linux__
    TEST_SKIP("Linux-only test");
#else
    using namespace broapps;

    std::shared_ptr<AppCatalog> catalog = AppCatalog::create({{}, true});
    TEST_CHECK(catalog != nullptr);

    auto watcher = CatalogWatcher::create(catalog);
    TEST_CHECK(watcher != nullptr);
    TEST_CHECK(!watcher->is_watching());

    bool ok = watcher->start();
    TEST_CHECK(ok);
    TEST_CHECK(watcher->is_watching());

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    watcher->stop();
    TEST_CHECK(!watcher->is_watching());

    std::cout << "test_linux_watcher passed!" << std::endl;
    return 0;
#endif
}
