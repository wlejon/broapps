#include "apps_folder.h"
#include "com_init.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlobj.h>
#include <propkey.h>
#include <propvarutil.h>
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

std::string sanitize_id(std::string_view name) {
    std::string id;
    for (char c : name) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-' || c == '!') {
            id.push_back(c);
        } else if (c == ' ') {
            id.push_back('.');
        }
    }
    return id.empty() ? "app" : id;
}

}  // namespace

std::vector<AppInfo> enumerate_apps_folder(bool include_nodisplay) {
    ComScope com;
    std::vector<AppInfo> apps;

    IShellItem* folder = nullptr;
    HRESULT hr = SHCreateItemFromParsingName(L"shell:AppsFolder", nullptr, IID_PPV_ARGS(&folder));
    if (FAILED(hr) || !folder) {
        return apps;
    }

    IEnumShellItems* enum_items = nullptr;
    hr = folder->BindToHandler(nullptr, BHID_EnumItems, IID_PPV_ARGS(&enum_items));
    if (FAILED(hr) || !enum_items) {
        folder->Release();
        return apps;
    }

    IShellItem* item = nullptr;
    while (enum_items->Next(1, &item, nullptr) == S_OK && item) {
        LPWSTR name_w = nullptr;
        item->GetDisplayName(SIGDN_NORMALDISPLAY, &name_w);

        LPWSTR parsing_w = nullptr;
        item->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &parsing_w);

        std::string name = name_w ? wide_to_utf8(name_w) : "";
        std::string parsing_name = parsing_w ? wide_to_utf8(parsing_w) : "";

        if (name_w) CoTaskMemFree(name_w);
        if (parsing_w) CoTaskMemFree(parsing_w);

        std::string aumid_str;
        std::string desc_str;

        IShellItem2* item2 = nullptr;
        if (SUCCEEDED(item->QueryInterface(IID_PPV_ARGS(&item2)))) {
            LPWSTR aumid_w = nullptr;
            if (SUCCEEDED(item2->GetString(PKEY_AppUserModel_ID, &aumid_w)) && aumid_w) {
                aumid_str = wide_to_utf8(aumid_w);
                CoTaskMemFree(aumid_w);
            }

            LPWSTR desc_w = nullptr;
            if (SUCCEEDED(item2->GetString(PKEY_FileDescription, &desc_w)) && desc_w) {
                desc_str = wide_to_utf8(desc_w);
                CoTaskMemFree(desc_w);
            }

            item2->Release();
        }

        item->Release();

        if (name.empty()) continue;

        AppInfo app;
        app.name = name;
        app.comment = desc_str;
        app.aumid = aumid_str;

        // An app is packaged if it has an AUMID with '!' (PackageFamilyName!AppId)
        if (!aumid_str.empty() && aumid_str.find('!') != std::string::npos) {
            app.is_packaged = true;
            app.id = aumid_str;
        } else if (!aumid_str.empty()) {
            app.id = aumid_str;
        } else if (!parsing_name.empty()) {
            app.id = sanitize_id(parsing_name);
        } else {
            app.id = sanitize_id(name);
        }

        if (parsing_name.find('\\') != std::string::npos && parsing_name.ends_with(".exe")) {
            app.executable_path = parsing_name;
        }

        // Filtering
        if (app.name.starts_with("Uninstall ") || app.name.ends_with(" Uninstall")) {
            app.is_nodisplay = true;
        }

        if (app.is_nodisplay && !include_nodisplay) {
            continue;
        }

        apps.push_back(std::move(app));
    }

    enum_items->Release();
    folder->Release();

    return apps;
}

}  // namespace broapps::win_backend
#endif
