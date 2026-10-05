#include "xbel_parser.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace broapps::linux_backend {

namespace {

std::string xml_escape(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        switch (c) {
            case '&': out.append("&amp;"); break;
            case '<': out.append("&lt;"); break;
            case '>': out.append("&gt;"); break;
            case '"': out.append("&quot;"); break;
            case '\'': out.append("&apos;"); break;
            default: out.push_back(c); break;
        }
    }
    return out;
}

std::string xml_unescape(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '&') {
            if (text.substr(i, 5) == "&amp;") { out.push_back('&'); i += 4; }
            else if (text.substr(i, 4) == "&lt;") { out.push_back('<'); i += 3; }
            else if (text.substr(i, 4) == "&gt;") { out.push_back('>'); i += 3; }
            else if (text.substr(i, 6) == "&quot;") { out.push_back('"'); i += 5; }
            else if (text.substr(i, 6) == "&apos;") { out.push_back('\''); i += 5; }
            else { out.push_back('&'); }
        } else {
            out.push_back(text[i]);
        }
    }
    return out;
}

std::string uri_decode(std::string_view uri) {
    std::string out;
    out.reserve(uri.size());
    for (size_t i = 0; i < uri.size(); ++i) {
        if (uri[i] == '%' && i + 2 < uri.size()) {
            char h1 = uri[i + 1];
            char h2 = uri[i + 2];
            int v1 = (h1 >= '0' && h1 <= '9') ? (h1 - '0') : ((h1 >= 'a' && h1 <= 'f') ? (h1 - 'a' + 10) : ((h1 >= 'A' && h1 <= 'F') ? (h1 - 'A' + 10) : -1));
            int v2 = (h2 >= '0' && h2 <= '9') ? (h2 - '0') : ((h2 >= 'a' && h2 <= 'f') ? (h2 - 'a' + 10) : ((h2 >= 'A' && h2 <= 'F') ? (h2 - 'A' + 10) : -1));
            if (v1 >= 0 && v2 >= 0) {
                out.push_back(static_cast<char>((v1 << 4) | v2));
                i += 2;
                continue;
            }
        }
        out.push_back(uri[i]);
    }
    return out;
}

std::string uri_encode_path(const std::filesystem::path& p) {
    std::string str = p.generic_string();
    std::string out = "file://";
    for (char c : str) {
        if (c == ' ') {
            out.append("%20");
        } else {
            out.push_back(c);
        }
    }
    return out;
}

std::string format_iso8601(std::chrono::system_clock::time_point tp) {
    std::time_t tt = std::chrono::system_clock::to_time_t(tp);
    std::tm gmt{};
#if defined(_WIN32)
    gmtime_s(&gmt, &tt);
#else
    gmtime_r(&tt, &gmt);
#endif
    std::ostringstream ss;
    ss << std::put_time(&gmt, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

std::string extract_attr(std::string_view tag, std::string_view attr_name) {
    std::string needle = std::string(attr_name) + "=\"";
    auto pos = tag.find(needle);
    if (pos == std::string::npos) {
        needle = std::string(attr_name) + "='";
        pos = tag.find(needle);
        if (pos == std::string::npos) return {};
    }

    size_t start = pos + needle.size();
    char quote = tag[start - 1];
    size_t end = tag.find(quote, start);
    if (end == std::string::npos) return {};

    return xml_unescape(tag.substr(start, end - start));
}

}  // namespace

std::vector<RecentItem> parse_xbel_string(std::string_view xml_content) {
    std::vector<RecentItem> items;

    size_t pos = 0;
    while ((pos = xml_content.find("<bookmark", pos)) != std::string::npos) {
        size_t end_tag = xml_content.find("</bookmark>", pos);
        if (end_tag == std::string::npos) {
            // Check self-closing
            end_tag = xml_content.find("/>", pos);
            if (end_tag == std::string::npos) break;
            end_tag += 2;
        } else {
            end_tag += 11;
        }

        std::string_view bookmark_block = xml_content.substr(pos, end_tag - pos);
        pos = end_tag;

        RecentItem item;
        item.uri = extract_attr(bookmark_block, "href");
        if (item.uri.starts_with("file://")) {
            std::string decoded_path = uri_decode(item.uri.substr(7));
            item.file_path = std::filesystem::path(decoded_path);
            item.display_name = item.file_path.filename().string();
        }

        // Title
        auto title_start = bookmark_block.find("<title>");
        if (title_start != std::string_view::npos) {
            auto title_end = bookmark_block.find("</title>", title_start);
            if (title_end != std::string_view::npos) {
                item.display_name = xml_unescape(bookmark_block.substr(title_start + 7, title_end - (title_start + 7)));
            }
        }

        // MIME type
        auto mime_start = bookmark_block.find("<mime:mime-type");
        if (mime_start != std::string_view::npos) {
            auto mime_end = bookmark_block.find("/>", mime_start);
            if (mime_end != std::string_view::npos) {
                item.mime_type = extract_attr(bookmark_block.substr(mime_start, mime_end - mime_start + 2), "type");
            }
        }

        // Application
        auto app_start = bookmark_block.find("<bookmark:application");
        if (app_start != std::string_view::npos) {
            auto app_end = bookmark_block.find("/>", app_start);
            if (app_end != std::string_view::npos) {
                item.app_id = extract_attr(bookmark_block.substr(app_start, app_end - app_start + 2), "name");
            }
        }

        if (!item.uri.empty()) {
            items.push_back(std::move(item));
        }
    }

    return items;
}

std::vector<RecentItem> parse_xbel_file(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) return {};
    std::ostringstream ss;
    ss << file.rdbuf();
    return parse_xbel_string(ss.str());
}

std::string serialize_xbel_string(const std::vector<RecentItem>& items) {
    std::ostringstream ss;
    ss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    ss << "<xbel version=\"1.0\"\n";
    ss << "      xmlns:bookmark=\"http://www.freedesktop.org/standards/desktop-bookmarks\"\n";
    ss << "      xmlns:mime=\"http://www.freedesktop.org/standards/shared-mime-info\">\n";

    for (const auto& item : items) {
        std::string uri = item.uri;
        if (uri.empty() && !item.file_path.empty()) {
            uri = uri_encode_path(item.file_path);
        }
        std::string iso_time = format_iso8601(item.timestamp);

        ss << "  <bookmark href=\"" << xml_escape(uri) << "\" added=\"" << iso_time << "\" modified=\"" << iso_time << "\" visited=\"" << iso_time << "\">\n";
        if (!item.display_name.empty()) {
            ss << "    <title>" << xml_escape(item.display_name) << "</title>\n";
        }
        ss << "    <info>\n";
        ss << "      <metadata owner=\"http://freedesktop.org\">\n";
        if (!item.mime_type.empty()) {
            ss << "        <mime:mime-type type=\"" << xml_escape(item.mime_type) << "\"/>\n";
        }
        if (!item.app_id.empty()) {
            ss << "        <bookmark:applications>\n";
            ss << "          <bookmark:application name=\"" << xml_escape(item.app_id) << "\" exec=\"&apos;" << xml_escape(item.app_id) << " %u&apos;\" modified=\"" << iso_time << "\" count=\"1\"/>\n";
            ss << "        </bookmark:applications>\n";
        }
        ss << "      </metadata>\n";
        ss << "    </info>\n";
        ss << "  </bookmark>\n";
    }

    ss << "</xbel>\n";
    return ss.str();
}

bool write_xbel_file(const std::string& file_path, const std::vector<RecentItem>& items) {
    std::filesystem::path target(file_path);
    std::error_code ec;
    std::filesystem::create_directories(target.parent_path(), ec);

    std::string tmp_path = file_path + ".tmp";
    {
        std::ofstream out(tmp_path, std::ios::trunc);
        if (!out.is_open()) return false;
        out << serialize_xbel_string(items);
    }

    std::filesystem::rename(tmp_path, target, ec);
    if (ec) {
        // Fallback copy
        std::filesystem::copy_file(tmp_path, target, std::filesystem::copy_options::overwrite_existing, ec);
        std::filesystem::remove(tmp_path, ec);
    }
    return !ec;
}

}  // namespace broapps::linux_backend
