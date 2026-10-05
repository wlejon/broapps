#include "plist_parser.h"

#if defined(__APPLE__)
#import <Foundation/Foundation.h>
#include <filesystem>

namespace broapps::mac_backend {

namespace {

std::string ns_to_string(NSString* str) {
    if (!str) return {};
    return std::string([str UTF8String]);
}

}  // namespace

std::optional<AppInfo> parse_bundle_info_plist(const std::string& app_bundle_path) {
    @autoreleasepool {
        std::filesystem::path bundle_p(app_bundle_path);
        std::filesystem::path plist_p = bundle_p / "Contents" / "Info.plist";

        std::error_code ec;
        if (!std::filesystem::exists(plist_p, ec)) {
            return std::nullopt;
        }

        NSString* plistPath = [NSString stringWithUTF8String:plist_p.string().c_str()];
        NSDictionary* dict = [NSDictionary dictionaryWithContentsOfFile:plistPath];
        if (!dict) {
            return std::nullopt;
        }

        AppInfo app;
        app.bundle_path = app_bundle_path;

        NSString* bundleId = dict[@"CFBundleIdentifier"];
        if (bundleId) {
            app.bundle_id = ns_to_string(bundleId);
            app.id = app.bundle_id;
        } else {
            app.id = bundle_p.stem().string();
        }

        NSString* displayName = dict[@"CFBundleDisplayName"];
        NSString* name = dict[@"CFBundleName"];

        if (displayName && [displayName length] > 0) {
            app.name = ns_to_string(displayName);
        } else if (name && [name length] > 0) {
            app.name = ns_to_string(name);
        } else {
            app.name = bundle_p.stem().string();
        }

        NSString* execName = dict[@"CFBundleExecutable"];
        if (execName) {
            std::filesystem::path exec_p = bundle_p / "Contents" / "MacOS" / [execName UTF8String];
            app.executable_path = exec_p.string();
        }

        NSString* iconFile = dict[@"CFBundleIconFile"];
        if (iconFile) {
            std::string icon_str = ns_to_string(iconFile);
            if (!icon_str.ends_with(".icns")) {
                icon_str += ".icns";
            }
            std::filesystem::path icon_p = bundle_p / "Contents" / "Resources" / icon_str;
            if (std::filesystem::exists(icon_p, ec)) {
                app.icon_name_or_path = icon_p.string();
            } else {
                app.icon_name_or_path = icon_str;
            }
        }

        // Check LSUIElement / LSBackgroundOnly
        id uiElement = dict[@"LSUIElement"];
        if (uiElement && [uiElement boolValue]) {
            app.is_nodisplay = true;
        }
        id bgOnly = dict[@"LSBackgroundOnly"];
        if (bgOnly && [bgOnly boolValue]) {
            app.is_nodisplay = true;
        }

        // CFBundleDocumentTypes -> MIME types
        id docTypes = dict[@"CFBundleDocumentTypes"];
        if ([docTypes isKindOfClass:[NSArray class]]) {
            for (NSDictionary* doc in (NSArray*)docTypes) {
                id mimes = doc[@"CFBundleTypeMIMETypes"];
                if ([mimes isKindOfClass:[NSArray class]]) {
                    for (NSString* m in (NSArray*)mimes) {
                        app.supported_mime_types.push_back(ns_to_string(m));
                    }
                }
                id exts = doc[@"CFBundleTypeExtensions"];
                if ([exts isKindOfClass:[NSArray class]]) {
                    for (NSString* ext in (NSArray*)exts) {
                        app.keywords.push_back(ns_to_string(ext));
                    }
                }
            }
        }

        // CFBundleURLTypes -> schemes
        id urlTypes = dict[@"CFBundleURLTypes"];
        if ([urlTypes isKindOfClass:[NSArray class]]) {
            for (NSDictionary* urlDict in (NSArray*)urlTypes) {
                id schemes = urlDict[@"CFBundleURLSchemes"];
                if ([schemes isKindOfClass:[NSArray class]]) {
                    for (NSString* s in (NSArray*)schemes) {
                        app.supported_protocols.push_back(ns_to_string(s));
                    }
                }
            }
        }

        // Categories
        NSString* cat = dict[@"LSApplicationCategoryType"];
        if (cat) {
            std::string c_str = ns_to_string(cat);
            if (c_str.starts_with("public.app-category.")) {
                c_str = c_str.substr(20);
            }
            app.categories.push_back(std::move(c_str));
        }

        return app;
    }
}

}  // namespace broapps::mac_backend
#endif
