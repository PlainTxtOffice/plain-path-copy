// Plain Path Copy settings app.
//
// The File Explorer command lives in plain-path-copy-shell.dll. This app adds
// that command to the context menu, sets how paths are quoted, and offers the
// feedback section every Plain Text Office app has.
//
// The installer runs it with --add-to-context-menu after installing the
// files and --remove-from-context-menu before removing them. Those switches
// show no window and report the outcome in the exit code.

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <winrt/base.h>

#include <string_view>

#include "package-registration.h"
#include "settings-window.h"

namespace {

constexpr std::wstring_view kAddSwitch = L"--add-to-context-menu";
constexpr std::wstring_view kRemoveSwitch = L"--remove-from-context-menu";

enum class Mode { kWindow, kAdd, kRemove, kUnknownSwitch };

Mode ModeFromCommandLine() {
  int count = 0;
  LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &count);
  if (arguments == nullptr) {
    return Mode::kWindow;
  }
  Mode mode = Mode::kWindow;
  if (count > 1) {
    const std::wstring_view argument = arguments[1];
    if (argument == kAddSwitch) {
      mode = Mode::kAdd;
    } else if (argument == kRemoveSwitch) {
      mode = Mode::kRemove;
    } else {
      mode = Mode::kUnknownSwitch;
    }
  }
  LocalFree(arguments);
  return mode;
}

// Adds or removes the package without a window. Returns 0 on success and 1
// on failure, so the installer can roll back a failed add.
int RunSilently(Mode mode) {
  winrt::init_apartment(winrt::apartment_type::multi_threaded);
  const package_registration::Result result =
      mode == Mode::kAdd ? package_registration::Add()
                         : package_registration::Remove();
  if (!result.succeeded) {
    OutputDebugStringW(result.message.c_str());
  }
  winrt::uninit_apartment();
  return result.succeeded ? 0 : 1;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE /*previous*/,
                    PWSTR /*command_line*/, int /*show*/) {
  const Mode mode = ModeFromCommandLine();
  if (mode == Mode::kAdd || mode == Mode::kRemove) {
    return RunSilently(mode);
  }
  if (mode == Mode::kUnknownSwitch) {
    return 2;
  }
  const INITCOMMONCONTROLSEX controls{sizeof(controls),
                                      ICC_STANDARD_CLASSES | ICC_LINK_CLASS};
  InitCommonControlsEx(&controls);
  winrt::init_apartment(winrt::apartment_type::single_threaded);
  settings_window::Run(instance);
  winrt::uninit_apartment();
  return 0;
}
