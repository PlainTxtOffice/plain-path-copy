#include "feedback.h"

#include <shellapi.h>
#include <winternl.h>

#include "app-info.h"

extern "C" NTSTATUS NTAPI RtlGetVersion(PRTL_OSVERSIONINFOW version);

namespace feedback {
namespace {

constexpr ULONG kFirstWindows11Build = 22000;

bool IsUnreserved(unsigned char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
         (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' ||
         c == '~';
}

std::string ToUtf8(std::wstring_view text) {
  if (text.empty()) {
    return {};
  }
  const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(),
                                       static_cast<int>(text.size()), nullptr,
                                       0, nullptr, nullptr);
  std::string result(static_cast<size_t>(size), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                      result.data(), size, nullptr, nullptr);
  return result;
}

std::wstring Architecture() {
  USHORT process = 0;
  USHORT native = 0;
  if (IsWow64Process2(GetCurrentProcess(), &process, &native)) {
    switch (native) {
      case IMAGE_FILE_MACHINE_AMD64:
        return L"x64";
      case IMAGE_FILE_MACHINE_ARM64:
        return L"ARM64";
      case IMAGE_FILE_MACHINE_I386:
        return L"x86";
      default:
        break;
    }
  }
  return L"unknown architecture";
}

bool Open(HWND owner, const std::wstring& url) {
  const auto result = reinterpret_cast<INT_PTR>(
      ShellExecuteW(owner, L"open", url.c_str(), nullptr, nullptr,
                    SW_SHOWNORMAL));
  return result > 32;
}

}  // namespace

std::wstring Subject(std::wstring_view app_name, Kind kind) {
  std::wstring subject(app_name);
  subject += kind == Kind::kBugReport ? L" bug report" : L" feature request";
  subject += L" (Windows)";
  return subject;
}

std::wstring FormatSystemSummary(std::wstring_view app_name,
                                 std::wstring_view version,
                                 std::wstring_view windows_release,
                                 std::wstring_view windows_build,
                                 std::wstring_view architecture) {
  std::wstring summary(app_name);
  summary += L' ';
  summary += version;
  summary += L"\nWindows ";
  summary += windows_release;
  summary += L" (";
  summary += windows_build;
  summary += L")\n";
  summary += architecture;
  return summary;
}

std::wstring SystemSummary() {
  RTL_OSVERSIONINFOW version{};
  version.dwOSVersionInfoSize = sizeof(version);
  RtlGetVersion(&version);
  const std::wstring release =
      version.dwBuildNumber >= kFirstWindows11Build ? L"11" : L"10";
  const std::wstring build = std::to_wstring(version.dwMajorVersion) + L"." +
                             std::to_wstring(version.dwMinorVersion) + L"." +
                             std::to_wstring(version.dwBuildNumber);
  return FormatSystemSummary(app_info::kName, app_info::kVersion, release,
                             build, Architecture());
}

std::wstring PercentEncode(std::wstring_view text) {
  constexpr wchar_t kHex[] = L"0123456789ABCDEF";
  std::wstring encoded;
  for (const char byte : ToUtf8(text)) {
    const auto c = static_cast<unsigned char>(byte);
    if (IsUnreserved(c)) {
      encoded += static_cast<wchar_t>(c);
    } else {
      encoded += L'%';
      encoded += kHex[c >> 4];
      encoded += kHex[c & 0x0F];
    }
  }
  return encoded;
}

std::wstring MailtoUrl(std::wstring_view app_name, Kind kind,
                       std::wstring_view summary) {
  std::wstring body = L"\r\n\r\n";
  for (const wchar_t c : summary) {
    if (c == L'\n') {
      body += L"\r\n";
    } else if (c != L'\r') {
      body += c;
    }
  }
  return std::wstring(L"mailto:") + kAddress +
         L"?subject=" + PercentEncode(Subject(app_name, kind)) +
         L"&body=" + PercentEncode(body);
}

bool OpenEmail(HWND owner, Kind kind) {
  return Open(owner, MailtoUrl(app_info::kName, kind, SystemSummary()));
}

bool OpenContactForm(HWND owner) { return Open(owner, kContactUrl); }

}  // namespace feedback
