// Turns Windows paths into the forward-slash text the command copies.
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace path_format {

// When the copied path is wrapped in double quotation marks. The numeric
// values are stored in the registry; never renumber them.
enum class QuoteMode : unsigned long {
  kWhenNeeded = 0,  // Only paths that contain whitespace.
  kAlways = 1,      // Every path, like Windows' own Copy as path.
  kNever = 2,       // No path.
};

inline constexpr QuoteMode kDefaultQuoteMode = QuoteMode::kWhenNeeded;

// Separates paths when several items are copied, as Copy as path does.
inline constexpr wchar_t kPathSeparator[] = L"\r\n";

// Returns true for a stored value that names a QuoteMode.
bool IsValidQuoteMode(unsigned long value);

// Replaces every backslash with a forward slash. Win32 file namespace
// prefixes are dropped first: \\?\C:\a becomes C:/a and \\?\UNC\server\share
// becomes //server/share.
std::wstring ToForwardSlashes(std::wstring_view windows_path);

// Returns true when the path contains whitespace, which splits an unquoted
// path into several arguments in a shell.
bool ContainsWhitespace(std::wstring_view path);

// Wraps the path in double quotation marks when the mode calls for it.
std::wstring ApplyQuoteMode(std::wstring_view path, QuoteMode mode);

// Converts and quotes each path, joined by kPathSeparator with no trailing
// separator. Returns an empty string for an empty list.
std::wstring FormatPaths(const std::vector<std::wstring>& windows_paths,
                         QuoteMode mode);

}  // namespace path_format
