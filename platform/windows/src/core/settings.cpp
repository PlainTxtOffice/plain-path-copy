#include "settings.h"

namespace settings {

path_format::QuoteMode ReadQuoteMode() {
  DWORD value = 0;
  DWORD size = sizeof(value);
  const LSTATUS status =
      RegGetValueW(HKEY_CURRENT_USER, kKeyPath, kQuoteModeValue,
                   RRF_RT_REG_DWORD, nullptr, &value, &size);
  if (status != ERROR_SUCCESS || !path_format::IsValidQuoteMode(value)) {
    return path_format::kDefaultQuoteMode;
  }
  return static_cast<path_format::QuoteMode>(value);
}

LSTATUS WriteQuoteMode(path_format::QuoteMode mode) {
  const DWORD value = static_cast<DWORD>(mode);
  return RegSetKeyValueW(HKEY_CURRENT_USER, kKeyPath, kQuoteModeValue,
                         REG_DWORD, &value, sizeof(value));
}

}  // namespace settings
