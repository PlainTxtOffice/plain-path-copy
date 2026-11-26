// Puts text on the Windows clipboard.
#pragma once

#include <windows.h>

#include <string>
#include <string_view>

namespace clipboard {

// Replaces the clipboard contents with Unicode text. Retries briefly while
// another program holds the clipboard open. Returns S_OK on success.
HRESULT SetText(std::wstring_view text, HWND owner);

// Returns the clipboard's Unicode text, or an empty string when it has
// none. Used by tests.
std::wstring GetText();

}  // namespace clipboard
