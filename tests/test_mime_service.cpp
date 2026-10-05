// MimeService takes file types from brovfs and only adds associations: its type answers equal
// brovfs's, and a file is typed by content as well as name (a PNG named notes.txt gets the
// image handlers). The extension table this used to carry is tested in brovfs (test_mime_db).
#include "broapps/app_catalog.h"
#include "broapps/mime_service.h"
#include "tests/test_common.h"

#include <brovfs/mime.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {

std::vector<std::string> ids(const std::vector<broapps::AppInfo>& apps) {
    std::vector<std::string> out;
    for (const auto& a : apps) out.push_back(a.id + "|" + a.executable_path);
    return out;
}

void write_bytes(const std::filesystem::path& p, const std::string& s) {
    std::ofstream f(p, std::ios::binary);
    f.write(s.data(), static_cast<std::streamsize>(s.size()));
}

}  // namespace

int main() {
    using namespace broapps;
    const auto& types = bro::vfs::MimeDatabase::system();

    std::shared_ptr<AppCatalog> catalog = AppCatalog::create({{}, true});
    auto svc = MimeService::create(catalog);
    TEST_CHECK(svc != nullptr);

    // Type queries are brovfs's answers.
    for (const char* ext : {".txt", "txt", "HTML", ".png", ".jpg", ".pdf", ".json", ".mp3", ".mp4", ".zip",
                            ".unknown_xyz_ext_123"}) {
        TEST_CHECK_EQ(svc->extension_to_mime(ext), types.type_for_extension(ext));
    }
    TEST_CHECK_EQ(svc->extension_to_mime(".txt"), "text/plain");
    TEST_CHECK_EQ(svc->extension_to_mime(".unknown_xyz_ext_123"), "application/octet-stream");
    auto png_exts = svc->mime_to_extensions("image/png");
    TEST_CHECK(std::find(png_exts.begin(), png_exts.end(), ".png") != png_exts.end());
    auto vfs_exts = types.extensions_for_type("text/html");
    auto html_exts = svc->mime_to_extensions("text/html");
    TEST_CHECK_EQ(html_exts.size(), vfs_exts.size());
    for (size_t i = 0; i < html_exts.size() && i < vfs_exts.size(); ++i) TEST_CHECK_EQ(html_exts[i], "." + vfs_exts[i]);

    // Files are typed by content and name.
    auto dir = std::filesystem::temp_directory_path() /
               ("broapps_mime_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(dir);
    const std::string png("\x89PNG\r\n\x1a\n\0\0\0\rIHDR\0\0\0\1\0\0\0\1\x08\x06\0\0\0", 26);
    write_bytes(dir / "notes.txt", png);
    write_bytes(dir / "readme.txt", "just words\n");
    TEST_CHECK_EQ(svc->mime_for_file(dir / "notes.txt"), "image/png");
    TEST_CHECK_EQ(svc->mime_for_file(dir / "readme.txt"), "text/plain");
    TEST_CHECK_EQ(svc->mime_for_file(dir / "missing.pdf"), types.type_for_extension("pdf"));
    TEST_CHECK(ids(svc->get_candidates_for_file(dir / "notes.txt")) == ids(svc->get_candidates_for_mime("image/png")));
    TEST_CHECK(ids(svc->get_candidates_for_file(dir / "readme.txt")) == ids(svc->get_candidates_for_mime("text/plain")));
    auto by_file = svc->get_default_app_for_file(dir / "notes.txt");
    auto by_mime = svc->get_default_app_for_mime("image/png");
    TEST_CHECK(by_file.has_value() == by_mime.has_value());
    if (by_file && by_mime) TEST_CHECK_EQ(by_file->id, by_mime->id);

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::cout << "test_mime_service passed (types from " << types.backend() << ")" << std::endl;
    return 0;
}
