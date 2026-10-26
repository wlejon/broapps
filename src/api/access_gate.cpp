// The helpers bro.apps's natives use to ask the host's access gate
// (embed/embed_access.h; what each native asks is in api.h, "Access").

#include "api.h"
#include "host_apps_internal.h"
#include "embed/embed_access.h"

namespace broapps::api {

std::filesystem::path utf8Path(const std::string& s) {
    return std::filesystem::path(std::u8string(s.begin(), s.end()));
}

std::string spawnRefusal(const std::string& what) {
    return ev::checkAccess(ev::AccessKind::Spawn, what);
}

std::string desktopReadRefusal(const std::string& what) {
    return ev::checkAccess(ev::AccessKind::DesktopRead, what);
}

std::string desktopWriteRefusal(const std::string& what) {
    return ev::checkAccess(ev::AccessKind::DesktopWrite, what);
}

std::string pathReadRefusal(const std::string& path, std::string* absolute) {
    if (!ev::accessGateActive()) return {};
    std::error_code ec;
    const std::filesystem::path p = utf8Path(path);
    std::filesystem::path abs = std::filesystem::absolute(p, ec);
    if (ec) abs = p;
    std::u8string u = abs.lexically_normal().u8string();
    *absolute = std::string(u.begin(), u.end());
    return ev::checkAccess(ev::AccessKind::Read, *absolute);
}

std::string launchDescription(const std::string& id, const std::vector<std::string>& args) {
    auto quoted = [](const std::string& s) {
        if (!s.empty() && s.find_first_of(" \t\n\"") == std::string::npos) return s;
        std::string q = "\"";
        for (char c : s) {
            if (c == '"' || c == '\\') q += '\\';
            q += c;
        }
        return q + "\"";
    };
    std::string line = quoted(id);
    for (const auto& a : args) {
        line += ' ';
        line += quoted(a);
    }
    return line;
}

Value newAccessError(const std::string& refusal, const std::string& path) {
    return ev::newAccessError(refusal, "EACCES", path);
}

Value throwAccessError(const std::string& refusal, const std::string& path) {
    return ev::throwAccessError(refusal, "EACCES", path);
}

} // namespace broapps::api
