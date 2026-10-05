#include "broapps/app_info.h"

#include <algorithm>
#include <cctype>

namespace broapps {

namespace {

bool contains_ci(std::string_view haystack, std::string_view needle) {
    if (needle.empty()) return true;
    if (haystack.size() < needle.size()) return false;

    auto it = std::search(
        haystack.begin(), haystack.end(),
        needle.begin(), needle.end(),
        [](char ch1, char ch2) {
            return std::tolower(static_cast<unsigned char>(ch1)) ==
                   std::tolower(static_cast<unsigned char>(ch2));
        });
    return it != haystack.end();
}

bool equals_ci(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

}  // namespace

bool AppInfo::matches_query(std::string_view query) const {
    if (query.empty()) return true;
    if (contains_ci(name, query)) return true;
    if (contains_ci(generic_name, query)) return true;
    if (contains_ci(id, query)) return true;
    if (contains_ci(comment, query)) return true;
    if (contains_ci(executable_path, query)) return true;

    for (const auto& kw : keywords) {
        if (contains_ci(kw, query)) return true;
    }
    for (const auto& cat : categories) {
        if (contains_ci(cat, query)) return true;
    }
    return false;
}

bool AppInfo::matches_category(std::string_view category) const {
    if (category.empty()) return true;
    for (const auto& cat : categories) {
        if (equals_ci(cat, category) || contains_ci(cat, category)) return true;
    }
    return false;
}

bool AppInfo::matches_mime_type(std::string_view mime_type) const {
    if (mime_type.empty()) return true;
    for (const auto& m : supported_mime_types) {
        if (equals_ci(m, mime_type)) return true;
        // Check wildcard e.g. "image/*" matching "image/png"
        std::string_view m_sv = m;
        if (mime_type.ends_with("/*")) {
            std::string_view prefix = mime_type.substr(0, mime_type.size() - 1);
            if (m_sv.starts_with(prefix)) return true;
        } else if (m_sv.ends_with("/*")) {
            std::string_view prefix = m_sv.substr(0, m_sv.size() - 1);
            if (mime_type.starts_with(prefix)) return true;
        }
    }
    return false;
}

}  // namespace broapps
