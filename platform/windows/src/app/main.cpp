// Plain Path Copy settings app.
//
// The File Explorer command lives in plain-path-copy-shell.dll. This app adds
// that command to the context menu, sets how paths are quoted, and offers the
// feedback section every Plain Text Office app has.

#include <windows.h>
#include <commctrl.h>
#include <winrt/base.h>

#include "settings-window.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE /*previous*/,
                    PWSTR /*command_line*/, int /*show*/) {
  const INITCOMMONCONTROLSEX controls{sizeof(controls),
                                      ICC_STANDARD_CLASSES | ICC_LINK_CLASS};
  InitCommonControlsEx(&controls);
  winrt::init_apartment(winrt::apartment_type::single_threaded);
  settings_window::Run(instance);
  winrt::uninit_apartment();
  return 0;
}
