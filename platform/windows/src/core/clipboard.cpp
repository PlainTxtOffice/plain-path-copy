#include "clipboard.h"

#include <cstring>

namespace clipboard {
namespace {

constexpr int kOpenAttempts = 10;
constexpr DWORD kOpenRetryDelayMs = 25;

// Opens the clipboard, waiting briefly for a clipboard manager or another
// program that has it open.
bool OpenWithRetry(HWND owner) {
  for (int attempt = 0; attempt < kOpenAttempts; ++attempt) {
    if (OpenClipboard(owner)) {
      return true;
    }
    Sleep(kOpenRetryDelayMs);
  }
  return false;
}

// Returns the thread's last Win32 error as an HRESULT, or E_FAIL when the
// failing call did not set one.
HRESULT LastErrorOrFail() {
  const DWORD error = GetLastError();
  return error == ERROR_SUCCESS ? E_FAIL : HRESULT_FROM_WIN32(error);
}

}  // namespace

HRESULT SetText(std::wstring_view text, HWND owner) {
  const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
  HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
  if (memory == nullptr) {
    return E_OUTOFMEMORY;
  }
  void* locked = GlobalLock(memory);
  if (locked == nullptr) {
    GlobalFree(memory);
    return E_OUTOFMEMORY;
  }
  auto* destination = static_cast<wchar_t*>(locked);
  std::memcpy(destination, text.data(), text.size() * sizeof(wchar_t));
  destination[text.size()] = L'\0';
  GlobalUnlock(memory);

  if (!OpenWithRetry(owner)) {
    const HRESULT error = LastErrorOrFail();
    GlobalFree(memory);
    return error;
  }
  HRESULT result = S_OK;
  if (!EmptyClipboard()) {
    result = LastErrorOrFail();
  } else if (SetClipboardData(CF_UNICODETEXT, memory) == nullptr) {
    result = LastErrorOrFail();
  } else {
    memory = nullptr;  // The clipboard owns it now.
  }
  CloseClipboard();
  if (memory != nullptr) {
    GlobalFree(memory);
  }
  return result;
}

std::wstring GetText() {
  std::wstring text;
  if (!OpenWithRetry(nullptr)) {
    return text;
  }
  HANDLE data = GetClipboardData(CF_UNICODETEXT);
  if (data != nullptr) {
    if (const auto* locked = static_cast<const wchar_t*>(GlobalLock(data))) {
      text = locked;
      GlobalUnlock(data);
    }
  }
  CloseClipboard();
  return text;
}

}  // namespace clipboard
