#pragma once

#ifdef _WIN32
#include <string>
#include <vector>

namespace broapps::win_backend {

struct WindowsAssociation {
    std::string default_progid;
    std::string default_executable;
    std::vector<std::string> candidate_progids;
    std::vector<std::string> candidate_executables;
};

// Queries Windows registry for file extension associations (.txt, .png, etc.)
WindowsAssociation query_registry_associations(std::string_view extension);

// Queries Windows registry for MIME type of a file extension
std::string query_registry_mime(std::string_view extension);

// Queries Windows registry for default extension of a MIME type
std::string query_registry_extension_for_mime(std::string_view mime_type);

// Extracts the executable path from a shell open command string
std::string extract_executable_from_command(std::string_view command_str);

}  // namespace broapps::win_backend
#endif
