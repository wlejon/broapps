#include "desktop_entry.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace broapps::linux_backend {

namespace {

std::string trim(std::string_view sv) {
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front()))) {
        sv.remove_prefix(1);
    }
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back()))) {
        sv.remove_suffix(1);
    }
    return std::string(sv);
}

std::vector<std::string> parse_semicolon_list(std::string_view val) {
    std::vector<std::string> out;
    std::string cur;
    bool escape = false;

    for (char c : val) {
        if (escape) {
            cur.push_back(c);
            escape = false;
        } else if (c == '\\') {
            escape = true;
        } else if (c == ';') {
            std::string item = trim(cur);
            if (!item.empty()) {
                out.push_back(std::move(item));
            }
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    std::string item = trim(cur);
    if (!item.empty()) {
        out.push_back(std::move(item));
    }
    return out;
}

std::string unescape_string_value(std::string_view val) {
    std::string out;
    out.reserve(val.size());
    for (size_t i = 0; i < val.size(); ++i) {
        if (val[i] == '\\' && i + 1 < val.size()) {
            char next = val[++i];
            switch (next) {
                case 's': out.push_back(' '); break;
                case 'n': out.push_back('\n'); break;
                case 't': out.push_back('\t'); break;
                case 'r': out.push_back('\r'); break;
                case '\\': out.push_back('\\'); break;
                case ';': out.push_back(';'); break;
                default:
                    out.push_back('\\');
                    out.push_back(next);
                    break;
            }
        } else {
            out.push_back(val[i]);
        }
    }
    return out;
}

bool parse_boolean(std::string_view val) {
    std::string t = trim(val);
    std::transform(t.begin(), t.end(), t.begin(), [](unsigned char c) { return std::tolower(c); });
    return (t == "true" || t == "1");
}

}  // namespace

std::optional<AppInfo> parse_desktop_entry_string(
    std::string_view content,
    std::string_view desktop_file_path,
    std::string_view entry_id) {

    AppInfo app;
    app.desktop_file_path = std::string(desktop_file_path);
    app.id = std::string(entry_id);

    enum class Group { None, Main, Action };
    Group current_group = Group::None;
    DesktopAction current_action;

    bool is_application_type = false;
    std::string type_val;

    std::istringstream stream((std::string(content)));
    std::string line;

    while (std::getline(stream, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed.front() == '#') continue;

        if (trimmed.front() == '[' && trimmed.back() == ']') {
            // New group
            if (current_group == Group::Action && !current_action.id.empty()) {
                app.actions.push_back(std::move(current_action));
                current_action = DesktopAction{};
            }

            std::string_view group_name = std::string_view(trimmed).substr(1, trimmed.size() - 2);
            if (group_name == "Desktop Entry") {
                current_group = Group::Main;
            } else if (group_name.starts_with("Desktop Action ")) {
                current_group = Group::Action;
                current_action.id = trim(group_name.substr(15));
            } else {
                current_group = Group::None;
            }
            continue;
        }

        auto eq_pos = trimmed.find('=');
        if (eq_pos == std::string::npos) continue;

        std::string key = trim(trimmed.substr(0, eq_pos));
        std::string raw_val = trimmed.substr(eq_pos + 1);

        // Strip locale tags for simplicity unless default
        if (key.find('[') != std::string::npos) {
            // Localized key, skip for now to prefer base unlocalized or handle later
            continue;
        }

        if (current_group == Group::Main) {
            if (key == "Type") {
                type_val = trim(raw_val);
                if (type_val == "Application") is_application_type = true;
            } else if (key == "Name") {
                app.name = unescape_string_value(raw_val);
            } else if (key == "GenericName") {
                app.generic_name = unescape_string_value(raw_val);
            } else if (key == "Comment") {
                app.comment = unescape_string_value(raw_val);
            } else if (key == "Icon") {
                app.icon_name_or_path = unescape_string_value(raw_val);
            } else if (key == "Exec") {
                app.exec_raw = trim(raw_val);
            } else if (key == "TryExec") {
                app.try_exec = unescape_string_value(raw_val);
            } else if (key == "Path") {
                app.working_directory = unescape_string_value(raw_val);
            } else if (key == "Terminal") {
                app.is_terminal = parse_boolean(raw_val);
            } else if (key == "NoDisplay") {
                if (parse_boolean(raw_val)) app.is_nodisplay = true;
            } else if (key == "Hidden") {
                if (parse_boolean(raw_val)) app.is_nodisplay = true;
            } else if (key == "Categories") {
                app.categories = parse_semicolon_list(raw_val);
            } else if (key == "Keywords") {
                app.keywords = parse_semicolon_list(raw_val);
            } else if (key == "MimeType") {
                app.supported_mime_types = parse_semicolon_list(raw_val);
            }
        } else if (current_group == Group::Action) {
            if (key == "Name") {
                current_action.name = unescape_string_value(raw_val);
            } else if (key == "Exec") {
                current_action.exec = trim(raw_val);
            } else if (key == "Icon") {
                current_action.icon = unescape_string_value(raw_val);
            }
        }
    }

    if (current_group == Group::Action && !current_action.id.empty()) {
        app.actions.push_back(std::move(current_action));
    }

    if (!is_application_type) {
        return std::nullopt;
    }

    if (app.id.empty() && !desktop_file_path.empty()) {
        try {
            app.id = std::filesystem::path(desktop_file_path).filename().string();
        } catch (...) {
            app.id = std::string(desktop_file_path);
        }
    }

    // Determine executable_path from Exec or TryExec
    if (!app.try_exec.empty()) {
        app.executable_path = app.try_exec;
    } else if (!app.exec_raw.empty()) {
        // First token of Exec
        LaunchScope dummy_scope;
        auto tokens = tokenize_and_expand_exec(app.exec_raw, app, dummy_scope);
        if (!tokens.empty()) {
            app.executable_path = tokens.front();
        }
    }

    return app;
}

std::optional<AppInfo> parse_desktop_entry_file(
    const std::string& filepath,
    std::string_view entry_id) {

    std::ifstream file(filepath, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        return std::nullopt;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();

    std::string id = std::string(entry_id);
    if (id.empty()) {
        try {
            id = std::filesystem::path(filepath).filename().string();
        } catch (...) {
            id = filepath;
        }
    }

    return parse_desktop_entry_string(content, filepath, id);
}

std::vector<std::string> tokenize_and_expand_exec(
    std::string_view exec_line,
    const AppInfo& app,
    const LaunchScope& scope) {

    std::vector<std::string> args;
    std::string current;
    bool in_double_quote = false;
    bool had_file_field_code = false;

    auto push_current = [&] {
        if (!current.empty()) {
            args.push_back(std::move(current));
            current.clear();
        }
    };

    size_t i = 0;
    while (i < exec_line.size()) {
        char c = exec_line[i];

        if (in_double_quote) {
            if (c == '"') {
                in_double_quote = false;
                ++i;
            } else if (c == '\\' && i + 1 < exec_line.size()) {
                char next = exec_line[i + 1];
                if (next == '"' || next == '\\' || next == '$' || next == '`') {
                    current.push_back(next);
                    i += 2;
                } else {
                    current.push_back('\\');
                    current.push_back(next);
                    i += 2;
                }
            } else if (c == '%' && i + 1 < exec_line.size()) {
                char code = exec_line[i + 1];
                i += 2;
                if (code == '%') {
                    current.push_back('%');
                } else if (code == 'f' || code == 'u') {
                    had_file_field_code = true;
                    if (!scope.files_to_open.empty()) {
                        current.append(scope.files_to_open.front());
                    }
                } else if (code == 'c') {
                    current.append(app.name);
                } else if (code == 'k') {
                    current.append(app.desktop_file_path);
                }
                // Deprecated/unsupported codes inside quotes are omitted
            } else {
                current.push_back(c);
                ++i;
            }
        } else {
            // Outside double quote
            if (std::isspace(static_cast<unsigned char>(c))) {
                push_current();
                ++i;
            } else if (c == '"') {
                in_double_quote = true;
                ++i;
            } else if (c == '\\' && i + 1 < exec_line.size()) {
                current.push_back(exec_line[i + 1]);
                i += 2;
            } else if (c == '%' && i + 1 < exec_line.size()) {
                char code = exec_line[i + 1];
                i += 2;
                if (code == '%') {
                    current.push_back('%');
                } else if (code == 'f') {
                    had_file_field_code = true;
                    push_current();
                    if (!scope.files_to_open.empty()) {
                        args.push_back(scope.files_to_open.front());
                    }
                } else if (code == 'F') {
                    had_file_field_code = true;
                    push_current();
                    for (const auto& file : scope.files_to_open) {
                        args.push_back(file);
                    }
                } else if (code == 'u') {
                    had_file_field_code = true;
                    push_current();
                    if (!scope.files_to_open.empty()) {
                        args.push_back(scope.files_to_open.front());
                    }
                } else if (code == 'U') {
                    had_file_field_code = true;
                    push_current();
                    for (const auto& url : scope.files_to_open) {
                        args.push_back(url);
                    }
                } else if (code == 'd') {
                    had_file_field_code = true;
                    push_current();
                    if (!scope.files_to_open.empty()) {
                        try {
                            args.push_back(std::filesystem::path(scope.files_to_open.front()).parent_path().string());
                        } catch (...) {}
                    }
                } else if (code == 'D') {
                    had_file_field_code = true;
                    push_current();
                    for (const auto& file : scope.files_to_open) {
                        try {
                            args.push_back(std::filesystem::path(file).parent_path().string());
                        } catch (...) {}
                    }
                } else if (code == 'n') {
                    had_file_field_code = true;
                    push_current();
                    if (!scope.files_to_open.empty()) {
                        try {
                            args.push_back(std::filesystem::path(scope.files_to_open.front()).filename().string());
                        } catch (...) {}
                    }
                } else if (code == 'N') {
                    had_file_field_code = true;
                    push_current();
                    for (const auto& file : scope.files_to_open) {
                        try {
                            args.push_back(std::filesystem::path(file).filename().string());
                        } catch (...) {}
                    }
                } else if (code == 'i') {
                    if (!app.icon_name_or_path.empty()) {
                        push_current();
                        args.push_back("--icon");
                        args.push_back(app.icon_name_or_path);
                    }
                } else if (code == 'c') {
                    current.append(app.name);
                } else if (code == 'k') {
                    current.append(app.desktop_file_path);
                }
                // Deprecated/unsupported codes like %v, %m are dropped
            } else {
                current.push_back(c);
                ++i;
            }
        }
    }
    push_current();

    // If no field code for files was used, append files_to_open at the end
    if (!had_file_field_code && !scope.files_to_open.empty()) {
        for (const auto& f : scope.files_to_open) {
            args.push_back(f);
        }
    }

    // Append extra arguments from scope
    for (const auto& arg : scope.arguments) {
        args.push_back(arg);
    }

    return args;
}

std::vector<std::string> expand_exec(
    const AppInfo& app,
    const LaunchScope& scope) {

    std::string exec_line = app.exec_raw;

    // Check if an action is requested
    if (!scope.action_id.empty()) {
        for (const auto& act : app.actions) {
            if (act.id == scope.action_id) {
                exec_line = act.exec;
                break;
            }
        }
    }

    return tokenize_and_expand_exec(exec_line, app, scope);
}

}  // namespace broapps::linux_backend
