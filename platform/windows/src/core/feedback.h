// Drafts bug reports and feature requests to Plain Text Office.
//
// Mirrors the feedback screens of the other Plain Text Office apps: the email
// draft carries only a subject and a short app and system summary, and
// nothing is sent unless the user sends it from their email app. Never add
// file paths, clipboard text, settings, or other user content.
#pragma once

#include <windows.h>

#include <string>
#include <string_view>

namespace feedback {

inline constexpr wchar_t kAddress[] = L"support@plaintxtoffice.com";
inline constexpr wchar_t kContactUrl[] = L"https://plaintxtoffice.com/contact/";

enum class Kind { kBugReport, kFeatureRequest };

// Returns the draft subject, such as "Plain Path Copy bug report (Windows)".
std::wstring Subject(std::wstring_view app_name, Kind kind);

// Returns the three-line app, Windows, and architecture summary.
std::wstring FormatSystemSummary(std::wstring_view app_name,
                                 std::wstring_view version,
                                 std::wstring_view windows_release,
                                 std::wstring_view windows_build,
                                 std::wstring_view architecture);

// Returns the summary for this app on this computer.
std::wstring SystemSummary();

// Percent-encodes UTF-8 bytes of the text, keeping only RFC 3986 unreserved
// characters.
std::wstring PercentEncode(std::wstring_view text);

// Returns a mailto: URL whose body is blank lines for the user's message
// followed by the summary, with CRLF line breaks as RFC 6068 requires.
std::wstring MailtoUrl(std::wstring_view app_name, Kind kind,
                       std::wstring_view summary);

// Opens a draft in the user's email app. Returns false when none opens it.
bool OpenEmail(HWND owner, Kind kind);

// Opens the contact form in the browser. Returns false when it fails.
bool OpenContactForm(HWND owner);

}  // namespace feedback
