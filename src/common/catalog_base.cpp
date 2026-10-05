#include "catalog_base.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>

namespace broapps {

namespace {

std::string to_lower_str(std::string_view sv) {
    std::string out;
    out.reserve(sv.size());
    for (char c : sv) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

std::vector<std::string> split_terms(std::string_view query) {
    std::vector<std::string> terms;
    std::string current;
    for (char c : query) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!current.empty()) {
                terms.push_back(to_lower_str(current));
                current.clear();
            }
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) {
        terms.push_back(to_lower_str(current));
    }
    return terms;
}

bool contains_term(std::string_view haystack, std::string_view term_lower) {
    if (term_lower.empty()) return true;
    if (haystack.size() < term_lower.size()) return false;

    auto it = std::search(
        haystack.begin(), haystack.end(),
        term_lower.begin(), term_lower.end(),
        [](char ch1, char ch2) {
            return std::tolower(static_cast<unsigned char>(ch1)) ==
                   std::tolower(static_cast<unsigned char>(ch2));
        });
    return it != haystack.end();
}

bool starts_with_term(std::string_view str, std::string_view term_lower) {
    if (str.size() < term_lower.size()) return false;
    for (size_t i = 0; i < term_lower.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(str[i])) !=
            std::tolower(static_cast<unsigned char>(term_lower[i]))) {
            return false;
        }
    }
    return true;
}

}  // namespace

int score_app(const AppInfo& app, const std::vector<std::string>& terms, std::string_view full_query) {
    if (terms.empty()) return 0;

    std::string full_query_lower = to_lower_str(full_query);
    std::string name_lower = to_lower_str(app.name);
    std::string id_lower = to_lower_str(app.id);

    // Exact matches
    if (id_lower == full_query_lower) return 1000;
    if (name_lower == full_query_lower) return 800;

    // Starts with full query
    int base_score = 0;
    if (starts_with_term(name_lower, full_query_lower)) {
        base_score += 400;
    } else if (starts_with_term(id_lower, full_query_lower)) {
        base_score += 300;
    }

    std::string exe_filename;
    if (!app.executable_path.empty()) {
        try {
            exe_filename = to_lower_str(std::filesystem::path(app.executable_path).filename().string());
        } catch (...) {
            exe_filename = to_lower_str(app.executable_path);
        }
    }

    int term_score = 0;
    for (const auto& term : terms) {
        int single_term_score = 0;
        if (name_lower == term) {
            single_term_score = std::max(single_term_score, 250);
        } else if (starts_with_term(name_lower, term)) {
            single_term_score = std::max(single_term_score, 180);
        } else if (contains_term(name_lower, term)) {
            single_term_score = std::max(single_term_score, 120);
        }

        if (starts_with_term(exe_filename, term)) {
            single_term_score = std::max(single_term_score, 100);
        } else if (contains_term(exe_filename, term)) {
            single_term_score = std::max(single_term_score, 60);
        }

        if (contains_term(app.generic_name, term)) {
            single_term_score = std::max(single_term_score, 70);
        }

        for (const auto& kw : app.keywords) {
            if (to_lower_str(kw) == term) {
                single_term_score = std::max(single_term_score, 80);
            } else if (contains_term(kw, term)) {
                single_term_score = std::max(single_term_score, 40);
            }
        }

        for (const auto& cat : app.categories) {
            if (to_lower_str(cat) == term) {
                single_term_score = std::max(single_term_score, 50);
            } else if (contains_term(cat, term)) {
                single_term_score = std::max(single_term_score, 25);
            }
        }

        if (contains_term(app.comment, term)) {
            single_term_score = std::max(single_term_score, 15);
        }

        if (single_term_score == 0) {
            // A required term didn't match anything
            return 0;
        }
        term_score += single_term_score;
    }

    return base_score + term_score;
}

CatalogBase::CatalogBase(CatalogConfig config)
    : config_(std::move(config)) {}

std::vector<std::filesystem::path> CatalogBase::source_directories() const {
    std::vector<std::filesystem::path> out;
    for (const auto& p : config_.extra_search_paths) out.emplace_back(p);
    return out;
}

void CatalogBase::set_apps(std::vector<AppInfo> apps) {
    std::lock_guard<std::mutex> lock(mutex_);
    apps_ = std::move(apps);
}

const std::vector<AppInfo>& CatalogBase::apps() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return apps_;
}

std::optional<AppInfo> CatalogBase::find_by_id(std::string_view id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& app : apps_) {
        if (app.id == id) {
            return app;
        }
    }
    // Also check case-insensitive match
    for (const auto& app : apps_) {
        if (to_lower_str(app.id) == to_lower_str(id)) {
            return app;
        }
    }
    return std::nullopt;
}

std::vector<AppInfo> CatalogBase::search(std::string_view query) const {
    std::vector<std::string> terms = split_terms(query);
    if (terms.empty()) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (config_.include_nodisplay) return apps_;
        std::vector<AppInfo> visible;
        for (const auto& a : apps_) {
            if (!a.is_nodisplay) visible.push_back(a);
        }
        return visible;
    }

    struct ScoredApp {
        int score = 0;
        const AppInfo* app = nullptr;
    };

    std::vector<ScoredApp> scored;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        scored.reserve(apps_.size());
        for (const auto& app : apps_) {
            if (app.is_nodisplay && !config_.include_nodisplay) continue;
            int s = score_app(app, terms, query);
            if (s > 0) {
                scored.push_back({s, &app});
            }
        }
    }

    std::sort(scored.begin(), scored.end(), [](const ScoredApp& a, const ScoredApp& b) {
        if (a.score != b.score) return a.score > b.score;
        return a.app->name < b.app->name;
    });

    std::vector<AppInfo> result;
    result.reserve(scored.size());
    for (const auto& item : scored) {
        result.push_back(*item.app);
    }
    return result;
}

std::vector<AppInfo> CatalogBase::find_by_category(std::string_view category) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<AppInfo> result;
    for (const auto& app : apps_) {
        if (app.is_nodisplay && !config_.include_nodisplay) continue;
        if (app.matches_category(category)) {
            result.push_back(app);
        }
    }
    return result;
}

std::vector<AppInfo> CatalogBase::find_by_mime_type(std::string_view mime_type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<AppInfo> result;
    for (const auto& app : apps_) {
        if (app.is_nodisplay && !config_.include_nodisplay) continue;
        if (app.matches_mime_type(mime_type)) {
            result.push_back(app);
        }
    }
    return result;
}

}  // namespace broapps
