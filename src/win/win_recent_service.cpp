#include "win_recent_service.h"
#include "lnk_parser.h"
#include "com_init.h"
#include "src/common/mime_table.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlobj.h>
#include <algorithm>
#include <filesystem>

namespace broapps::win_backend {

namespace {

std::wstring utf8_to_wide(std::string_view str) {
    if (str.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
    if (size <= 0) return {};
    std::wstring out(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), out.data(), size);
    return out;
}

std::filesystem::path get_recent_folder() {
    PWSTR recent_path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Recent, 0, nullptr, &recent_path)) && recent_path) {
        std::filesystem::path p(recent_path);
        CoTaskMemFree(recent_path);
        return p;
    }
    return {};
}

}  // namespace

std::vector<RecentItem> WinRecentService::get_recent_items(size_t limit) {
    std::lock_guard<std::mutex> lock(mutex_);
    ComScope com;
    std::vector<RecentItem> results;

    auto recent_dir = get_recent_folder();
    std::error_code ec;
    if (recent_dir.empty() || !std::filesystem::exists(recent_dir, ec)) {
        return results;
    }

    struct EntryWithTime {
        std::filesystem::path lnk_path;
        std::filesystem::file_time_type mtime;
    };

    std::vector<EntryWithTime> entries;
    for (auto it = std::filesystem::directory_iterator(recent_dir, std::filesystem::directory_options::skip_permission_denied, ec);
         it != std::filesystem::directory_iterator(); it.increment(ec)) {
        if (ec) continue;
        if (it->is_regular_file(ec) && it->path().extension() == L".lnk") {
            entries.push_back({it->path(), it->last_write_time(ec)});
        }
    }

    std::sort(entries.begin(), entries.end(), [](const EntryWithTime& a, const EntryWithTime& b) {
        return a.mtime > b.mtime;
    });

    for (const auto& e : entries) {
        if (results.size() >= limit) break;

        auto parsed = parse_lnk_file(e.lnk_path.wstring());
        if (!parsed || parsed->executable_path.empty()) continue;

        RecentItem item;
        item.file_path = std::filesystem::path(parsed->executable_path);
        item.display_name = parsed->name;
        item.uri = "file:///" + item.file_path.generic_string();
        item.mime_type = lookup_mime_by_extension(item.file_path.extension().string());

        // File timestamp
        auto s_tp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            e.mtime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
        item.timestamp = s_tp;

        results.push_back(std::move(item));
    }

    return results;
}

bool WinRecentService::add_recent_item(const std::filesystem::path& file_path, const std::string& app_id) {
    (void)app_id;
    std::lock_guard<std::mutex> lock(mutex_);
    std::wstring p_w = utf8_to_wide(file_path.string());
    SHAddToRecentDocs(SHARD_PATHW, p_w.c_str());
    return true;
}

bool WinRecentService::clear_recent_items() {
    std::lock_guard<std::mutex> lock(mutex_);
    SHAddToRecentDocs(SHARD_PATHW, nullptr);
    return true;
}

}  // namespace broapps::win_backend

namespace broapps {

std::unique_ptr<RecentService> RecentService::create() {
    return std::make_unique<win_backend::WinRecentService>();
}

}  // namespace broapps
#endif
