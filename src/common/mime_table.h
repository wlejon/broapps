#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace broapps {

// Resolves a file extension (e.g. ".txt" or "txt") to a MIME type
std::string lookup_mime_by_extension(std::string_view extension);

// Resolves a MIME type (e.g. "image/png") to list of known file extensions (e.g. {".png"})
std::vector<std::string> lookup_extensions_by_mime(std::string_view mime_type);

}  // namespace broapps
