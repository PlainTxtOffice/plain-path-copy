#include "path-format.h"

#include <algorithm>
#include <cwctype>

namespace path_format {
namespace {

constexpr std::wstring_view kUncFileNamespace = L"\\\\?\\UNC\\";
constexpr std::wstring_view kFileNamespace = L"\\\\?\\";

// Drops a \\?\ prefix so tools that do not understand it get a plain path.
// A \\?\ prefix that is not followed by a drive or UNC path, such as a
// volume GUID path, is kept because nothing shorter names the same place.
std::wstring StripFileNamespace(std::wstring_view path) {
  if (path.starts_with(kUncFileNamespace)) {
    return L"\\\\" + std::wstring(path.substr(kUncFileNamespace.size()));
  }
  if (path.starts_with(kFileNamespace)) {
    const std::wstring_view rest = path.substr(kFileNamespace.size());
    const bool has_drive = rest.size() >= 2 && rest[1] == L':' &&
                           std::iswalpha(rest[0]) != 0;
    if (has_drive) {
      return std::wstring(rest);
    }
  }
  return std::wstring(path);
}

}  // namespace

bool IsValidQuoteMode(unsigned long value) {
  return value <= static_cast<unsigned long>(QuoteMode::kNever);
}

std::wstring ToForwardSlashes(std::wstring_view windows_path) {
  std::wstring result = StripFileNamespace(windows_path);
  std::replace(result.begin(), result.end(), L'\\', L'/');
  return result;
}

bool ContainsWhitespace(std::wstring_view path) {
  return std::any_of(path.begin(), path.end(),
                     [](wchar_t c) { return std::iswspace(c) != 0; });
}

std::wstring ApplyQuoteMode(std::wstring_view path, QuoteMode mode) {
  const bool quote = mode == QuoteMode::kAlways ||
                     (mode == QuoteMode::kWhenNeeded && ContainsWhitespace(path));
  if (!quote) {
    return std::wstring(path);
  }
  std::wstring result;
  result.reserve(path.size() + 2);
  result += L'"';
  result += path;
  result += L'"';
  return result;
}

std::wstring FormatPaths(const std::vector<std::wstring>& windows_paths,
                         QuoteMode mode) {
  std::wstring result;
  bool first = true;
  for (const std::wstring& path : windows_paths) {
    if (!first) {
      result += kPathSeparator;
    }
    first = false;
    result += ApplyQuoteMode(ToForwardSlashes(path), mode);
  }
  return result;
}

}  // namespace path_format
