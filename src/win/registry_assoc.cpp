#include "registry_assoc.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <algorithm>
#include <cctype>

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

std::string wide_to_utf8(std::wstring_view wstr) {
    if (wstr.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string out(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), out.data(), size, nullptr, nullptr);
    return out;
}

std::string read_reg_string(HKEY root, const std::wstring& subkey, const wchar_t* value_name) {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(root, subkey.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return {};
    }

    wchar_t buf[1024] = {};
    DWORD buf_bytes = sizeof(buf);
    DWORD type = 0;
    std::string out;

    if (RegQueryValueExW(hKey, value_name, nullptr, &type, reinterpret_cast<LPBYTE>(buf), &buf_bytes) == ERROR_SUCCESS) {
        if (type == REG_SZ || type == REG_EXPAND_SZ) {
            out = wide_to_utf8(buf);
        }
    }

    RegCloseKey(hKey);
    return out;
}

std::vector<std::string> read_reg_value_names(HKEY root, const std::wstring& subkey) {
    std::vector<std::string> names;
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(root, subkey.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return names;
    }

    wchar_t val_buf[256] = {};
    DWORD val_len = 256;
    DWORD index = 0;

    while (RegEnumValueW(hKey, index, val_buf, &val_len, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
        names.push_back(wide_to_utf8(val_buf));
        val_len = 256;
        ++index;
    }

    RegCloseKey(hKey);
    return names;
}

}  // namespace

std::string extract_executable_from_command(std::string_view command_str) {
    while (!command_str.empty() && std::isspace(static_cast<unsigned char>(command_str.front()))) {
        command_str.remove_prefix(1);
    }
    if (command_str.empty()) return {};

    if (command_str.front() == '"') {
        auto end_q = command_str.find('"', 1);
        if (end_q != std::string_view::npos) {
            return std::string(command_str.substr(1, end_q - 1));
        }
    }

    auto space_pos = command_str.find(' ');
    if (space_pos != std::string_view::npos) {
        return std::string(command_str.substr(0, space_pos));
    }
    return std::string(command_str);
}

static std::string get_executable_for_progid(const std::string& progid) {
    if (progid.empty()) return {};
    std::wstring subkey = utf8_to_wide(progid) + L"\\shell\\open\\command";
    std::string cmd = read_reg_string(HKEY_CLASSES_ROOT, subkey, nullptr);
    if (!cmd.empty()) {
        return extract_executable_from_command(cmd);
    }
    return {};
}

WindowsAssociation query_registry_associations(std::string_view extension) {
    WindowsAssociation assoc;
    std::string ext = std::string(extension);
    if (!ext.starts_with('.')) ext = "." + ext;

    std::wstring ext_w = utf8_to_wide(ext);

    // 1. UserChoice (highest precedence on Windows 8/10/11)
    std::wstring user_choice_key = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\" + ext_w + L"\\UserChoice";
    assoc.default_progid = read_reg_string(HKEY_CURRENT_USER, user_choice_key, L"ProgId");

    // 2. If no UserChoice, check HKCR\<ext> default
    if (assoc.default_progid.empty()) {
        assoc.default_progid = read_reg_string(HKEY_CLASSES_ROOT, ext_w, nullptr);
    }

    if (!assoc.default_progid.empty()) {
        assoc.default_executable = get_executable_for_progid(assoc.default_progid);
    }

    // 3. OpenWithProgids
    std::wstring open_with_progids_key = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\" + ext_w + L"\\OpenWithProgids";
    for (const auto& name : read_reg_value_names(HKEY_CURRENT_USER, open_with_progids_key)) {
        if (!name.empty() && name != assoc.default_progid) {
            assoc.candidate_progids.push_back(name);
            std::string exe = get_executable_for_progid(name);
            if (!exe.empty()) {
                assoc.candidate_executables.push_back(std::move(exe));
            }
        }
    }

    // 4. OpenWithList
    std::wstring open_with_list_key = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\" + ext_w + L"\\OpenWithList";
    for (const auto& name : read_reg_value_names(HKEY_CURRENT_USER, open_with_list_key)) {
        if (name != "MRUList") {
            std::string exe_val = read_reg_string(HKEY_CURRENT_USER, open_with_list_key, utf8_to_wide(name).c_str());
            if (!exe_val.empty()) {
                assoc.candidate_executables.push_back(std::move(exe_val));
            }
        }
    }

    return assoc;
}

}  // namespace broapps::win_backend
#endif
