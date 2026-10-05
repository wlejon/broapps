#include "lnk_parser.h"
#include "com_init.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <filesystem>
#include <algorithm>
#include <cctype>

namespace broapps::win_backend {

namespace {

std::string wide_to_utf8(std::wstring_view wstr) {
    if (wstr.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string out(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), out.data(), size, nullptr, nullptr);
    return out;
}

std::wstring utf8_to_wide(std::string_view str) {
    if (str.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
    if (size <= 0) return {};
    std::wstring out(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), out.data(), size);
    return out;
}

std::string sanitize_id(std::string_view name) {
    std::string id;
    for (char c : name) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-') {
            id.push_back(c);
        } else if (c == ' ') {
            id.push_back('.');
        }
    }
    return id.empty() ? "app" : id;
}

bool is_uninstaller(std::string_view name, std::string_view target_path) {
    std::string n;
    n.reserve(name.size());
    for (char c : name) n.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));

    if (n.starts_with("uninstall") || n.ends_with("uninstall") || n.find("unins") != std::string::npos) {
        return true;
    }

    std::string t;
    t.reserve(target_path.size());
    for (char c : target_path) t.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));

    if (t.find("unins000.exe") != std::string::npos || t.find("uninstall.exe") != std::string::npos) {
        return true;
    }

    return false;
}

}  // namespace

std::optional<AppInfo> parse_lnk_file(const std::wstring& lnk_path) {
    IShellLinkW* psl = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, reinterpret_cast<void**>(&psl));
    if (FAILED(hr) || !psl) {
        return std::nullopt;
    }

    IPersistFile* ppf = nullptr;
    hr = psl->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&ppf));
    if (FAILED(hr) || !ppf) {
        psl->Release();
        return std::nullopt;
    }

    hr = ppf->Load(lnk_path.c_str(), STGM_READ);
    if (FAILED(hr)) {
        ppf->Release();
        psl->Release();
        return std::nullopt;
    }

    wchar_t target_buf[MAX_PATH] = {};
    WIN32_FIND_DATAW wfd = {};
    psl->GetPath(target_buf, MAX_PATH, &wfd, SLGP_UNCPRIORITY);

    wchar_t args_buf[1024] = {};
    psl->GetArguments(args_buf, 1024);

    wchar_t workdir_buf[MAX_PATH] = {};
    psl->GetWorkingDirectory(workdir_buf, MAX_PATH);

    wchar_t desc_buf[1024] = {};
    psl->GetDescription(desc_buf, 1024);

    wchar_t icon_buf[MAX_PATH] = {};
    int icon_index = 0;
    psl->GetIconLocation(icon_buf, MAX_PATH, &icon_index);

    ppf->Release();
    psl->Release();

    std::filesystem::path p(lnk_path);
    std::string filename_stem = wide_to_utf8(p.stem().wstring());
    std::string target_path = wide_to_utf8(target_buf);

    AppInfo app;
    app.name = filename_stem;
    app.shortcut_path = wide_to_utf8(lnk_path);
    app.executable_path = target_path;
    app.arguments = wide_to_utf8(args_buf);
    app.working_directory = wide_to_utf8(workdir_buf);
    app.comment = wide_to_utf8(desc_buf);

    if (icon_buf[0] != L'\0') {
        app.icon_name_or_path = wide_to_utf8(icon_buf);
        if (icon_index > 0) {
            app.icon_name_or_path += "," + std::to_string(icon_index);
        }
    } else if (!target_path.empty()) {
        app.icon_name_or_path = target_path;
    }

    app.id = sanitize_id(filename_stem);

    // Heuristics
    if (is_uninstaller(app.name, app.executable_path)) {
        app.is_nodisplay = true;
    }

    // If target has no extension or is url
    if (target_path.ends_with(".url") || target_path.starts_with("http://") || target_path.starts_with("https://")) {
        app.is_nodisplay = true;
    }

    return app;
}

std::vector<std::wstring> get_start_menu_dirs(const std::vector<std::string>& extra_paths) {
    std::vector<std::wstring> dirs;

    for (const auto& ep : extra_paths) {
        dirs.push_back(utf8_to_wide(ep));
    }

    PWSTR common_progs = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_CommonPrograms, 0, nullptr, &common_progs)) && common_progs) {
        dirs.emplace_back(common_progs);
        CoTaskMemFree(common_progs);
    }

    PWSTR user_progs = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Programs, 0, nullptr, &user_progs)) && user_progs) {
        dirs.emplace_back(user_progs);
        CoTaskMemFree(user_progs);
    }

    return dirs;
}

std::vector<AppInfo> scan_start_menu_shortcuts(
    const std::vector<std::string>& extra_paths,
    bool include_nodisplay) {

    ComScope com;
    std::vector<AppInfo> results;
    auto dirs = get_start_menu_dirs(extra_paths);

    for (const auto& dir : dirs) {
        std::error_code ec;
        std::filesystem::path base_path(dir);
        if (!std::filesystem::exists(base_path, ec) || !std::filesystem::is_directory(base_path, ec)) {
            continue;
        }

        for (auto it = std::filesystem::recursive_directory_iterator(base_path, std::filesystem::directory_options::skip_permission_denied, ec);
             it != std::filesystem::recursive_directory_iterator();
             it.increment(ec)) {

            if (ec) continue;
            if (it->is_regular_file(ec) && it->path().extension() == L".lnk") {
                auto app = parse_lnk_file(it->path().wstring());
                if (!app) continue;

                if (app->is_nodisplay && !include_nodisplay) {
                    continue;
                }

                results.push_back(std::move(*app));
            }
        }
    }

    return results;
}

}  // namespace broapps::win_backend
#endif
