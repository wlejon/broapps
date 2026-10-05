#include "mac_mime_service.h"
#include "plist_parser.h"

#if defined(__APPLE__)
#import <CoreServices/CoreServices.h>
#import <Foundation/Foundation.h>
#include <unordered_set>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

namespace broapps::mac_backend {

namespace {

std::string cf_to_string(CFStringRef cf) {
    if (!cf) return {};
    char buf[1024];
    if (CFStringGetCString(cf, buf, sizeof(buf), kCFStringEncodingUTF8)) {
        return std::string(buf);
    }
    return {};
}

std::string uti_from_mime(std::string_view mime) {
    CFStringRef mime_cf = CFStringCreateWithCString(nullptr, std::string(mime).c_str(), kCFStringEncodingUTF8);
    if (!mime_cf) return {};
    CFStringRef uti_cf = UTTypeCreatePreferredIdentifierForTag(kUTTagClassMIMEType, mime_cf, nullptr);
    CFRelease(mime_cf);
    if (!uti_cf) return {};
    std::string uti = cf_to_string(uti_cf);
    CFRelease(uti_cf);
    return uti;
}

std::string uti_from_ext(std::string_view ext) {
    if (ext.starts_with('.')) ext.remove_prefix(1);
    CFStringRef ext_cf = CFStringCreateWithCString(nullptr, std::string(ext).c_str(), kCFStringEncodingUTF8);
    if (!ext_cf) return {};
    CFStringRef uti_cf = UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, ext_cf, nullptr);
    CFRelease(ext_cf);
    if (!uti_cf) return {};
    std::string uti = cf_to_string(uti_cf);
    CFRelease(uti_cf);
    return uti;
}

std::string mime_from_uti(std::string_view uti) {
    CFStringRef uti_cf = CFStringCreateWithCString(nullptr, std::string(uti).c_str(), kCFStringEncodingUTF8);
    if (!uti_cf) return {};
    CFStringRef mime_cf = UTTypeCopyPreferredTagWithClass(uti_cf, kUTTagClassMIMEType);
    CFRelease(uti_cf);
    if (!mime_cf) return {};
    std::string mime = cf_to_string(mime_cf);
    CFRelease(mime_cf);
    return mime;
}

}  // namespace

MacMimeService::MacMimeService(std::shared_ptr<AppCatalog> catalog)
    : MimeServiceBase(std::move(catalog)) {}

std::string MacMimeService::extension_to_mime(std::string_view extension) const {
    std::string uti = uti_from_ext(extension);
    if (!uti.empty()) {
        std::string m = mime_from_uti(uti);
        if (!m.empty()) return m;
    }
    return lookup_mime_by_extension(extension);
}

std::vector<std::string> MacMimeService::mime_to_extensions(std::string_view mime_type) const {
    std::string uti = uti_from_mime(mime_type);
    if (!uti.empty()) {
        CFStringRef uti_cf = CFStringCreateWithCString(nullptr, uti.c_str(), kCFStringEncodingUTF8);
        if (uti_cf) {
            CFStringRef ext_cf = UTTypeCopyPreferredTagWithClass(uti_cf, kUTTagClassFilenameExtension);
            CFRelease(uti_cf);
            if (ext_cf) {
                std::string ext = cf_to_string(ext_cf);
                CFRelease(ext_cf);
                if (!ext.empty()) {
                    return {"." + ext};
                }
            }
        }
    }
    return lookup_extensions_by_mime(mime_type);
}

std::optional<AppInfo> MacMimeService::get_default_app_for_mime(std::string_view mime_type) {
    std::string uti = uti_from_mime(mime_type);
    if (!uti.empty()) {
        CFStringRef uti_cf = CFStringCreateWithCString(nullptr, uti.c_str(), kCFStringEncodingUTF8);
        if (uti_cf) {
            CFStringRef bundle_id_cf = LSCopyDefaultRoleHandlerForContentType(uti_cf, kLSRolesAll);
            CFRelease(uti_cf);
            if (bundle_id_cf) {
                std::string bundle_id = cf_to_string(bundle_id_cf);
                CFRelease(bundle_id_cf);

                auto app = find_app_in_catalog(bundle_id);
                if (app) return app;
            }
        }
    }

    auto cands = get_candidates_for_mime(mime_type);
    if (!cands.empty()) return cands.front();

    return std::nullopt;
}

std::vector<AppInfo> MacMimeService::get_candidates_for_mime(std::string_view mime_type) {
    std::vector<AppInfo> results;
    std::unordered_set<std::string> seen_bundle_ids;

    std::string uti = uti_from_mime(mime_type);
    if (!uti.empty()) {
        CFStringRef uti_cf = CFStringCreateWithCString(nullptr, uti.c_str(), kCFStringEncodingUTF8);
        if (uti_cf) {
            CFArrayRef handlers = LSCopyAllRoleHandlersForContentType(uti_cf, kLSRolesAll);
            CFRelease(uti_cf);
            if (handlers) {
                CFIndex count = CFArrayGetCount(handlers);
                for (CFIndex i = 0; i < count; ++i) {
                    auto b_cf = (CFStringRef)CFArrayGetValueAtIndex(handlers, i);
                    std::string bundle_id = cf_to_string(b_cf);
                    if (!bundle_id.empty() && !seen_bundle_ids.contains(bundle_id)) {
                        seen_bundle_ids.insert(bundle_id);
                        auto app = find_app_in_catalog(bundle_id);
                        if (app) {
                            results.push_back(std::move(*app));
                        }
                    }
                }
                CFRelease(handlers);
            }
        }
    }

    // Also include any catalog apps claiming support for this MIME type
    for (const auto& a : filter_catalog_by_mime(mime_type)) {
        if (!seen_bundle_ids.contains(a.id) && !seen_bundle_ids.contains(a.bundle_id)) {
            seen_bundle_ids.insert(a.id);
            results.push_back(a);
        }
    }

    return results;
}

}  // namespace broapps::mac_backend

namespace broapps {

std::unique_ptr<MimeService> MimeService::create(std::shared_ptr<AppCatalog> catalog) {
    return std::make_unique<mac_backend::MacMimeService>(std::move(catalog));
}

}  // namespace broapps
#pragma clang diagnostic pop
#endif
