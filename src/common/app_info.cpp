#include "broapps/app_info.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>

namespace broapps {

namespace {

bool read_text_file(const std::filesystem::path& p, std::string& out) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

std::string extract_json_string_field(const std::string& json, const std::string& key) {
    std::string pat = "\"" + key + "\"";
    size_t pos = json.find(pat);
    if (pos == std::string::npos) return {};
    size_t colon = json.find(':', pos + pat.size());
    if (colon == std::string::npos) return {};
    size_t q1 = json.find('"', colon + 1);
    if (q1 == std::string::npos) return {};
    size_t q2 = json.find('"', q1 + 1);
    if (q2 == std::string::npos) return {};
    return json.substr(q1 + 1, q2 - q1 - 1);
}

bool extract_json_string_array(const std::string& json, const std::string& key,
                               std::vector<std::string>& out) {
    std::string pat = "\"" + key + "\"";
    size_t pos = json.find(pat);
    if (pos == std::string::npos) return false;
    size_t colon = json.find(':', pos + pat.size());
    if (colon == std::string::npos) return false;
    size_t open_br = json.find('[', colon + 1);
    if (open_br == std::string::npos) return false;
    size_t close_br = json.find(']', open_br + 1);
    if (close_br == std::string::npos) return false;

    size_t cur = open_br + 1;
    while (cur < close_br) {
        size_t q1 = json.find('"', cur);
        if (q1 == std::string::npos || q1 >= close_br) break;
        size_t q2 = json.find('"', q1 + 1);
        if (q2 == std::string::npos || q2 >= close_br) break;
        out.push_back(json.substr(q1 + 1, q2 - q1 - 1));
        cur = q2 + 1;
    }
    return true;
}

bool json_has_literal_field(const std::string& json, const std::string& key, const std::string& lit) {
    std::string pat = "\"" + key + "\"";
    size_t pos = json.find(pat);
    if (pos == std::string::npos) return false;
    size_t colon = json.find(':', pos + pat.size());
    if (colon == std::string::npos) return false;
    size_t cur = colon + 1;
    while (cur < json.size() &&
           (json[cur] == ' ' || json[cur] == '\t' || json[cur] == '\r' || json[cur] == '\n')) {
        ++cur;
    }
    if (json.compare(cur, lit.size(), lit) != 0) return false;
    size_t after = cur + lit.size();
    if (after < json.size()) {
        char c = json[after];
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') return false;
    }
    return true;
}

bool extract_json_uint_field(const std::string& json, const std::string& key, uint32_t& out) {
    std::string pat = "\"" + key + "\"";
    size_t pos = json.find(pat);
    if (pos == std::string::npos) return false;
    size_t colon = json.find(':', pos + pat.size());
    if (colon == std::string::npos) return false;
    size_t cur = colon + 1;
    while (cur < json.size() &&
           (json[cur] == ' ' || json[cur] == '\t' || json[cur] == '\r' || json[cur] == '\n')) {
        ++cur;
    }
    if (cur >= json.size() || !std::isdigit(static_cast<unsigned char>(json[cur]))) return false;
    uint32_t val = 0;
    while (cur < json.size() && std::isdigit(static_cast<unsigned char>(json[cur]))) {
        val = val * 10 + static_cast<uint32_t>(json[cur] - '0');
        ++cur;
    }
    out = val;
    return true;
}

bool is_privileged_host_global(std::string_view cap) {
    return cap == "child_process" || cap == "fs" || cap == "process" ||
           cap == "require" || cap == "eval" || cap == "Function" ||
           cap == "bro.sys" || cap == "bro.cred" || cap == "bro.seat" ||
           cap == "bro.portal" || cap == "bro.displays" ||
           cap == "bro.compositor" || cap == "bro.wl" ||
           cap == "bro.fs" || cap == "bro.process" ||
           cap.rfind("__bro_native", 0) == 0;
}

bool is_capability_allowed(const std::string& cap, const std::set<std::string>& allowed_caps) {
    if (allowed_caps.count(cap) != 0) return true;
    for (const auto& ac : allowed_caps) {
        if (cap.size() > ac.size() && cap.compare(0, ac.size(), ac) == 0 && cap[ac.size()] == '.') {
            return true;
        }
    }
    return false;
}

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

std::string compute_fnv1a64_hex(std::string_view file_path, bool* ok_out) {
    std::string bytes;
    if (!read_text_file(std::filesystem::path(file_path), bytes)) {
        if (ok_out) *ok_out = false;
        return {};
    }
    if (ok_out) *ok_out = true;
    const auto* data = reinterpret_cast<const uint8_t*>(bytes.data());
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < bytes.size(); ++i) {
        hash ^= static_cast<uint64_t>(data[i]);
        hash *= 0x100000001b3ULL;
    }
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(16) << hash;
    return oss.str();
}

AppletCertificateInfo inspect_applet_certificate(std::string_view app_dir_or_cert_path) {
    AppletCertificateInfo info;
    if (app_dir_or_cert_path.empty()) return info;

    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path p(app_dir_or_cert_path);
    fs::path app_dir;
    fs::path cert_path;

    if (fs::is_regular_file(p, ec) && p.extension() == ".json") {
        cert_path = p;
        app_dir = p.parent_path();
    } else {
        app_dir = p;
        cert_path = app_dir / "app.cert.json";
    }

    if (app_dir.empty()) app_dir = ".";

    std::string bro_json;
    bool has_manifest_caps = false;
    bool manifest_requires_cert = false;
    std::set<std::string> allowed_caps;
    if (read_text_file(app_dir / "bro.json", bro_json)) {
        manifest_requires_cert = json_has_literal_field(bro_json, "verifiedApplet", "true");
        std::vector<std::string> caps;
        if (extract_json_string_array(bro_json, "capabilities", caps)) {
            has_manifest_caps = true;
            allowed_caps.insert(caps.begin(), caps.end());
        }
        std::vector<std::string> perms;
        if (extract_json_string_array(bro_json, "permissions", perms)) {
            has_manifest_caps = true;
            allowed_caps.insert(perms.begin(), perms.end());
        }
        info.manifest_capabilities.assign(allowed_caps.begin(), allowed_caps.end());
    }

    std::string cert_json;
    if (!read_text_file(cert_path, cert_json)) {
        if (manifest_requires_cert) {
            info.error = "missing applet verification certificate (" + cert_path.string() + ")";
        }
        return info;
    }

    info.present = true;
    info.cert_path = cert_path.string();
    extract_json_uint_field(cert_json, "hazards", info.hazards);
    info.proofs_valid = json_has_literal_field(cert_json, "proofsValid", "true");
    info.module_hash = extract_json_string_field(cert_json, "moduleHash");
    extract_json_string_array(cert_json, "capabilities", info.capabilities);

    if (!json_has_literal_field(cert_json, "verified", "true")) {
        info.error = "applet certificate does not have \"verified\": true";
        return info;
    }
    if (!json_has_literal_field(cert_json, "hazards", "0") || info.hazards != 0) {
        info.error = "applet certificate reports non-zero static hazards";
        return info;
    }
    if (!info.proofs_valid) {
        info.error = "applet certificate reports invalid IL proofs";
        return info;
    }
    if (info.module_hash.empty()) {
        info.error = "applet certificate is missing moduleHash";
        return info;
    }

    // Check module hash against compiled module (app.so/app.dll/app.dylib) or entry script (main.js/index.js) if present
    const char* bin_candidates[] = {"app.so", "app.dll", "app.dylib"};
    fs::path matched_bin;
    for (const char* name : bin_candidates) {
        fs::path cand = app_dir / name;
        if (fs::is_regular_file(cand, ec)) {
            matched_bin = cand;
            break;
        }
    }
    if (!matched_bin.empty()) {
        bool hash_ok = false;
        std::string actual_hash = compute_fnv1a64_hex(matched_bin.string(), &hash_ok);
        if (!hash_ok || actual_hash != info.module_hash) {
            info.error = "applet certificate hash mismatch for " + matched_bin.filename().string() +
                         " (cert has '" + info.module_hash + "', actual is '" + actual_hash + "')";
            return info;
        }
    } else {
        const char* js_candidates[] = {"main.js", "index.js"};
        for (const char* name : js_candidates) {
            fs::path cand = app_dir / name;
            if (fs::is_regular_file(cand, ec)) {
                bool hash_ok = false;
                std::string actual_hash = compute_fnv1a64_hex(cand.string(), &hash_ok);
                if (!hash_ok || actual_hash != info.module_hash) {
                    info.error = "applet certificate hash mismatch for " + cand.filename().string() +
                                 " (cert has '" + info.module_hash + "', actual is '" + actual_hash + "')";
                    return info;
                }
                break;
            }
        }
    }

    for (const auto& cap : info.capabilities) {
        if (!is_capability_allowed(cap, allowed_caps) &&
            (is_privileged_host_global(cap) || has_manifest_caps)) {
            info.error = "applet certificate requests unauthorized capability '" + cap + "'";
            return info;
        }
    }

    info.verified = true;
    return info;
}

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
