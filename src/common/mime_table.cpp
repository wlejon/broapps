#include "mime_table.h"

#include <algorithm>
#include <cctype>
#include <string_view>

namespace broapps {

namespace {

struct MimeEntry {
    std::string_view extension;
    std::string_view mime_type;
};

// Static table of common extensions to MIME types
constexpr MimeEntry kMimeTable[] = {
    // Text
    {"txt", "text/plain"},
    {"text", "text/plain"},
    {"log", "text/plain"},
    {"html", "text/html"},
    {"htm", "text/html"},
    {"css", "text/css"},
    {"csv", "text/csv"},
    {"xml", "text/xml"},
    {"md", "text/markdown"},
    {"markdown", "text/markdown"},
    {"c", "text/x-c"},
    {"cpp", "text/x-c++"},
    {"cxx", "text/x-c++"},
    {"cc", "text/x-c++"},
    {"h", "text/x-c++hdr"},
    {"hpp", "text/x-c++hdr"},
    {"py", "text/x-python"},
    {"rs", "text/rust"},
    {"go", "text/x-go"},
    {"java", "text/x-java"},
    {"js", "application/javascript"},
    {"mjs", "application/javascript"},
    {"json", "application/json"},
    {"sh", "application/x-sh"},
    {"bat", "application/x-bat"},
    {"ps1", "application/x-powershell"},
    {"yaml", "application/x-yaml"},
    {"yml", "application/x-yaml"},
    {"toml", "application/toml"},
    {"ini", "text/plain"},
    {"conf", "text/plain"},

    // Images
    {"png", "image/png"},
    {"jpg", "image/jpeg"},
    {"jpeg", "image/jpeg"},
    {"jpe", "image/jpeg"},
    {"gif", "image/gif"},
    {"svg", "image/svg+xml"},
    {"webp", "image/webp"},
    {"bmp", "image/bmp"},
    {"ico", "image/x-icon"},
    {"tif", "image/tiff"},
    {"tiff", "image/tiff"},
    {"avif", "image/avif"},
    {"heic", "image/heic"},
    {"psd", "image/vnd.adobe.photoshop"},

    // Audio
    {"mp3", "audio/mpeg"},
    {"wav", "audio/wav"},
    {"ogg", "audio/ogg"},
    {"oga", "audio/ogg"},
    {"opus", "audio/opus"},
    {"flac", "audio/flac"},
    {"aac", "audio/aac"},
    {"m4a", "audio/mp4"},
    {"mid", "audio/midi"},
    {"midi", "audio/midi"},

    // Video
    {"mp4", "video/mp4"},
    {"m4v", "video/mp4"},
    {"mkv", "video/x-matroska"},
    {"webm", "video/webm"},
    {"avi", "video/x-msvideo"},
    {"mov", "video/quicktime"},
    {"wmv", "video/x-ms-wmv"},
    {"flv", "video/x-flv"},

    // Documents
    {"pdf", "application/pdf"},
    {"epub", "application/epub+zip"},
    {"doc", "application/msword"},
    {"docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
    {"xls", "application/vnd.ms-excel"},
    {"xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
    {"ppt", "application/vnd.ms-powerpoint"},
    {"pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
    {"odt", "application/vnd.oasis.opendocument.text"},
    {"ods", "application/vnd.oasis.opendocument.spreadsheet"},
    {"odp", "application/vnd.oasis.opendocument.presentation"},

    // Archives
    {"zip", "application/zip"},
    {"tar", "application/x-tar"},
    {"gz", "application/gzip"},
    {"bz2", "application/x-bzip2"},
    {"xz", "application/x-xz"},
    {"7z", "application/x-7z-compressed"},
    {"rar", "application/vnd.rar"},

    // Fonts
    {"ttf", "font/ttf"},
    {"otf", "font/otf"},
    {"woff", "font/woff"},
    {"woff2", "font/woff2"},
};

std::string normalize_ext(std::string_view ext) {
    if (ext.starts_with('.')) {
        ext.remove_prefix(1);
    }
    std::string s;
    s.reserve(ext.size());
    for (char c : ext) {
        s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return s;
}

std::string normalize_mime(std::string_view mime) {
    std::string s;
    s.reserve(mime.size());
    for (char c : mime) {
        s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return s;
}

}  // namespace

std::string lookup_mime_by_extension(std::string_view extension) {
    std::string norm = normalize_ext(extension);
    if (norm.empty()) return "application/octet-stream";

    for (const auto& entry : kMimeTable) {
        if (entry.extension == norm) {
            return std::string(entry.mime_type);
        }
    }
    return "application/octet-stream";
}

std::vector<std::string> lookup_extensions_by_mime(std::string_view mime_type) {
    std::string norm = normalize_mime(mime_type);
    std::vector<std::string> out;

    for (const auto& entry : kMimeTable) {
        if (entry.mime_type == norm) {
            out.push_back("." + std::string(entry.extension));
        }
    }
    return out;
}

}  // namespace broapps
