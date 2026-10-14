#include "win_icon_resolver.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace broapps::win_backend {

namespace {

std::wstring utf8_to_wide(std::string_view str) {
    if (str.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
    if (size <= 0) return {};
    std::wstring out(size_t(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), out.data(), size);
    return out;
}

std::wstring expand_env(const std::wstring& in) {
    if (in.find(L'%') == std::wstring::npos) return in;
    DWORD needed = ExpandEnvironmentStringsW(in.c_str(), nullptr, 0);
    if (needed == 0) return in;
    std::wstring out(size_t(needed), L'\0');
    DWORD written = ExpandEnvironmentStringsW(in.c_str(), out.data(), needed);
    if (written == 0) return in;
    if (!out.empty() && out.back() == L'\0') out.pop_back();
    return out;
}

std::string to_lower_ascii(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool is_direct_image_ext(std::string_view ext) {
    return ext == ".png" || ext == ".ico" || ext == ".svg" || ext == ".bmp" || ext == ".jpg" || ext == ".jpeg";
}

uint32_t crc32_bytes(const uint8_t* data, size_t len, uint32_t crc = 0xFFFFFFFFu) {
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            crc = (crc >> 1) ^ (0xEDB88320u & (-(int32_t)(crc & 1)));
        }
    }
    return ~crc;
}

void append_be32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(v & 0xFF));
}

void append_chunk(std::vector<uint8_t>& out, const char type[4], const uint8_t* data, size_t len) {
    append_be32(out, static_cast<uint32_t>(len));
    size_t type_offset = out.size();
    out.insert(out.end(), reinterpret_cast<const uint8_t*>(type), reinterpret_cast<const uint8_t*>(type) + 4);
    if (len > 0) out.insert(out.end(), data, data + len);
    uint32_t crc = crc32_bytes(out.data() + type_offset, 4 + len);
    append_be32(out, crc);
}

bool write_rgba_png(const std::filesystem::path& path, int width, int height, const std::vector<uint8_t>& rgba) {
    if (width <= 0 || height <= 0 || rgba.size() < size_t(width) * size_t(height) * 4) return false;

    std::vector<uint8_t> raw;
    raw.reserve(size_t(height) * (size_t(width) * 4 + 1));
    for (int y = 0; y < height; ++y) {
        raw.push_back(0);  // filter type 0 (None)
        const uint8_t* row = rgba.data() + size_t(y) * size_t(width) * 4;
        raw.insert(raw.end(), row, row + size_t(width) * 4);
    }

    // Zlib stream with uncompressed deflate blocks (valid for any PNG decoder).
    std::vector<uint8_t> zdata;
    zdata.push_back(0x78);
    zdata.push_back(0x01);
    size_t pos = 0;
    while (pos < raw.size()) {
        size_t chunk = std::min<size_t>(65535, raw.size() - pos);
        bool final_block = (pos + chunk == raw.size());
        zdata.push_back(final_block ? 0x01 : 0x00);
        uint16_t len = static_cast<uint16_t>(chunk);
        uint16_t nlen = static_cast<uint16_t>(~len);
        zdata.push_back(static_cast<uint8_t>(len & 0xFF));
        zdata.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        zdata.push_back(static_cast<uint8_t>(nlen & 0xFF));
        zdata.push_back(static_cast<uint8_t>((nlen >> 8) & 0xFF));
        zdata.insert(zdata.end(), raw.data() + pos, raw.data() + pos + chunk);
        pos += chunk;
    }

    uint32_t s1 = 1, s2 = 0;
    for (uint8_t b : raw) {
        s1 = (s1 + b) % 65521u;
        s2 = (s2 + s1) % 65521u;
    }
    append_be32(zdata, (s2 << 16) | s1);

    std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    std::vector<uint8_t> ihdr;
    append_be32(ihdr, static_cast<uint32_t>(width));
    append_be32(ihdr, static_cast<uint32_t>(height));
    ihdr.push_back(8);  // bit depth
    ihdr.push_back(6);  // RGBA
    ihdr.push_back(0);  // compression
    ihdr.push_back(0);  // filter
    ihdr.push_back(0);  // interlace
    append_chunk(png, "IHDR", ihdr.data(), ihdr.size());
    append_chunk(png, "IDAT", zdata.data(), zdata.size());
    append_chunk(png, "IEND", nullptr, 0);

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
    return out.good();
}

bool extract_hicon_rgba(HICON hicon, int& out_w, int& out_h, std::vector<uint8_t>& out_rgba) {
    if (!hicon) return false;
    ICONINFO ii{};
    if (!GetIconInfo(hicon, &ii)) return false;
    auto cleanup = [&] {
        if (ii.hbmColor) DeleteObject(ii.hbmColor);
        if (ii.hbmMask) DeleteObject(ii.hbmMask);
    };

    int w = 0, h = 0;
    if (ii.hbmColor) {
        BITMAP bm{};
        if (GetObjectW(ii.hbmColor, sizeof(bm), &bm) && bm.bmWidth > 0 && bm.bmHeight > 0) {
            w = bm.bmWidth;
            h = bm.bmHeight;
        }
    } else if (ii.hbmMask) {
        BITMAP bm{};
        if (GetObjectW(ii.hbmMask, sizeof(bm), &bm) && bm.bmWidth > 0 && bm.bmHeight > 1) {
            w = bm.bmWidth;
            h = bm.bmHeight / 2;
        }
    }
    if (w <= 0 || h <= 0 || w > 512 || h > 512) {
        cleanup();
        return false;
    }

    HDC hdc = CreateCompatibleDC(nullptr);
    if (!hdc) {
        cleanup();
        return false;
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    std::vector<uint8_t> bgra(size_t(w) * size_t(h) * 4, 0);
    bool ok = false;
    if (ii.hbmColor && GetDIBits(hdc, ii.hbmColor, 0, UINT(h), bgra.data(), &bmi, DIB_RGB_COLORS) > 0) {
        ok = true;
        bool has_alpha = false;
        for (size_t i = 3; i < bgra.size(); i += 4) {
            if (bgra[i] != 0) {
                has_alpha = true;
                break;
            }
        }
        if (!has_alpha && ii.hbmMask) {
            std::vector<uint8_t> mask(size_t(w) * size_t(h) * 4, 0);
            if (GetDIBits(hdc, ii.hbmMask, 0, UINT(h), mask.data(), &bmi, DIB_RGB_COLORS) > 0) {
                for (size_t i = 0; i < bgra.size(); i += 4) {
                    bgra[i + 3] = (mask[i] == 0) ? 255 : 0;
                }
            } else {
                for (size_t i = 3; i < bgra.size(); i += 4) bgra[i] = 255;
            }
        }
    } else {
        void* bits = nullptr;
        HBITMAP dib = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (dib && bits) {
            HGDIOBJ old = SelectObject(hdc, dib);
            DrawIconEx(hdc, 0, 0, hicon, w, h, 0, nullptr, DI_NORMAL);
            GdiFlush();
            std::memcpy(bgra.data(), bits, bgra.size());
            for (size_t i = 0; i < bgra.size(); i += 4) {
                if (bgra[i + 3] == 0 && (bgra[i] != 0 || bgra[i + 1] != 0 || bgra[i + 2] != 0))
                    bgra[i + 3] = 255;
            }
            ok = true;
            SelectObject(hdc, old);
        }
        if (dib) DeleteObject(dib);
    }

    DeleteDC(hdc);
    cleanup();
    if (!ok) return false;

    out_w = w;
    out_h = h;
    out_rgba.resize(bgra.size());
    for (size_t i = 0; i < bgra.size(); i += 4) {
        out_rgba[i + 0] = bgra[i + 2];
        out_rgba[i + 1] = bgra[i + 1];
        out_rgba[i + 2] = bgra[i + 0];
        out_rgba[i + 3] = bgra[i + 3];
    }
    return true;
}

std::filesystem::path icon_cache_dir() {
    PWSTR local_app_data = nullptr;
    std::filesystem::path dir;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local_app_data)) && local_app_data) {
        dir = std::filesystem::path(local_app_data) / "broapps" / "icon-cache";
    }
    if (local_app_data) CoTaskMemFree(local_app_data);
    if (dir.empty()) dir = std::filesystem::temp_directory_path() / "broapps-icon-cache";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}

std::string fnv1a_hex(std::wstring_view key) {
    uint64_t h = 14695981039346656037ull;
    for (wchar_t wc : key) {
        h ^= static_cast<uint64_t>(wc & 0xFF);
        h *= 1099511628211ull;
        h ^= static_cast<uint64_t>((wc >> 8) & 0xFF);
        h *= 1099511628211ull;
    }
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(16) << h;
    return oss.str();
}

}  // namespace

std::optional<std::filesystem::path> WinIconResolver::resolve_icon(
    const std::string& icon_name_or_path,
    uint32_t preferred_size) {

    if (icon_name_or_path.empty()) return std::nullopt;

    std::string path_str = icon_name_or_path;
    int icon_index = 0;
    bool has_index = false;
    auto comma_pos = path_str.rfind(',');
    if (comma_pos != std::string::npos && comma_pos > 0 && comma_pos + 1 < path_str.size()) {
        std::string idx_part = path_str.substr(comma_pos + 1);
        bool valid_idx = true;
        for (size_t i = 0; i < idx_part.size(); ++i) {
            if (i == 0 && (idx_part[i] == '-' || idx_part[i] == '+')) continue;
            if (!std::isdigit(static_cast<unsigned char>(idx_part[i]))) {
                valid_idx = false;
                break;
            }
        }
        if (valid_idx) {
            try {
                icon_index = std::stoi(idx_part);
                has_index = true;
            } catch (...) {}
            path_str = path_str.substr(0, comma_pos);
        }
    }

    if (path_str.size() >= 2 && path_str.front() == '"' && path_str.back() == '"') {
        path_str = path_str.substr(1, path_str.size() - 2);
    }

    std::wstring wide_spec = expand_env(utf8_to_wide(path_str));
    std::error_code ec;
    std::filesystem::path resolved_path(wide_spec);
    if (!std::filesystem::exists(resolved_path, ec)) {
        wchar_t win_dir[MAX_PATH] = {};
        if (GetWindowsDirectoryW(win_dir, MAX_PATH)) {
            auto cand1 = std::filesystem::path(win_dir) / L"System32" / wide_spec;
            auto cand2 = std::filesystem::path(win_dir) / wide_spec;
            if (std::filesystem::exists(cand1, ec)) {
                resolved_path = cand1;
            } else if (std::filesystem::exists(cand2, ec)) {
                resolved_path = cand2;
            } else {
                return std::nullopt;
            }
        } else {
            return std::nullopt;
        }
    }

    std::string ext = to_lower_ascii(resolved_path.extension().string());
    if (is_direct_image_ext(ext) && !has_index) {
        return resolved_path;
    }

    int target_size = (preferred_size > 0 && preferred_size <= 256) ? static_cast<int>(preferred_size) : 48;
    std::wstring cache_key = resolved_path.wstring() + L"#" + std::to_wstring(icon_index) + L"#" + std::to_wstring(target_size);
    std::filesystem::path cached_png = icon_cache_dir() / (fnv1a_hex(cache_key) + ".png");
    if (std::filesystem::exists(cached_png, ec) && std::filesystem::file_size(cached_png, ec) > 0) {
        return cached_png;
    }

    HICON hicon = nullptr;
    std::wstring wpath = resolved_path.wstring();
    if (ext != ".lnk") {
        if (FAILED(SHDefExtractIconW(wpath.c_str(), icon_index, 0, &hicon, nullptr, MAKELONG(target_size, 0)))) {
            hicon = nullptr;
        }
        if (!hicon) {
            ExtractIconExW(wpath.c_str(), icon_index, &hicon, nullptr, 1);
        }
    }
    if (!hicon) {
        SHFILEINFOW sfi{};
        UINT flags = SHGFI_ICON | (target_size <= 24 ? SHGFI_SMALLICON : SHGFI_LARGEICON);
        if (SHGetFileInfoW(wpath.c_str(), 0, &sfi, sizeof(sfi), flags) && sfi.hIcon) {
            hicon = sfi.hIcon;
        }
    }

    if (hicon) {
        int w = 0, h = 0;
        std::vector<uint8_t> rgba;
        bool extracted = extract_hicon_rgba(hicon, w, h, rgba);
        DestroyIcon(hicon);
        if (extracted && write_rgba_png(cached_png, w, h, rgba)) {
            return cached_png;
        }
    }

    return resolved_path;
}

}  // namespace broapps::win_backend

namespace broapps {

std::unique_ptr<IconResolver> IconResolver::create() {
    return std::make_unique<win_backend::WinIconResolver>();
}

}  // namespace broapps
#endif
