// Reads and writes the user's settings under HKEY_CURRENT_USER.
#pragma once

#include <windows.h>

#include "path-format.h"

namespace settings {

// The per-user key, derived from the publisher and app slugs.
inline constexpr wchar_t kKeyPath[] =
    L"Software\\plain-text-office\\plain-path-copy";
inline constexpr wchar_t kQuoteModeValue[] = L"quote-mode";

// Returns the stored quote mode, or the default when the value is missing,
// unreadable, or out of range.
path_format::QuoteMode ReadQuoteMode();

// Stores the quote mode. Returns the Win32 error code, ERROR_SUCCESS on
// success.
LSTATUS WriteQuoteMode(path_format::QuoteMode mode);

}  // namespace settings
