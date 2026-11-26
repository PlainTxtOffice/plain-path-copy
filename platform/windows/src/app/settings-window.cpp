#include "settings-window.h"

#include <appmodel.h>
#include <commctrl.h>
#include <winrt/base.h>

#include <memory>
#include <string>
#include <thread>

#include "app-info.h"
#include "app-resource.h"
#include "feedback.h"
#include "package-registration.h"
#include "path-format.h"
#include "settings.h"

namespace settings_window {
namespace {

using package_registration::State;
using path_format::QuoteMode;

// Posted by the worker thread when adding or removing finishes. lParam owns
// a heap-allocated OperationOutcome.
constexpr UINT kOperationFinished = WM_APP + 1;

constexpr COLORREF kErrorColor = RGB(0xC4, 0x2B, 0x1C);

constexpr wchar_t kFeedbackIntro[] =
    L"Found a problem or have an idea? Report a Bug and Request a Feature "
    L"open a draft in your email app, addressed to Plain Text Office, with "
    L"your app version and Windows version filled in. Open Contact Form opens "
    L"the contact form in your browser. Nothing is sent unless you send it.";
constexpr wchar_t kEmailFailure[] =
    L"No email app is set up on this computer. Use Open Contact Form instead.";
constexpr wchar_t kContactFailure[] =
    L"The contact form couldn't be opened. Use Report a Bug or Request a "
    L"Feature to write an email instead.";

// Sample paths for the quotation mark preview.
constexpr wchar_t kPlainSample[] = L"C:\\work\\notes.txt";
constexpr wchar_t kSpacedSample[] = L"C:\\My Files\\notes.txt";

enum class Operation { kAdd, kRemove };

struct OperationOutcome {
  Operation operation;
  package_registration::Result result;
};

struct WindowState {
  bool busy = false;
  bool close_when_done = false;  // Close was chosen while busy.
  std::wstring last_error;  // Shown in the status until the next operation.
  std::thread worker;
};

// Returns true when this process runs with the package's identity, as it
// does when started from the Start menu. Removing the package then closes
// this process too.
bool RunningFromPackage() {
  UINT32 length = 0;
  return GetCurrentPackageFullName(&length, nullptr) ==
         ERROR_INSUFFICIENT_BUFFER;
}

// Keeps keyboard focus on an enabled control after buttons are disabled.
void KeepFocusUsable(HWND dialog) {
  HWND focus = GetFocus();
  if (focus != nullptr && IsWindowEnabled(focus)) {
    return;
  }
  for (int control : {IDC_ADD, IDC_REMOVE, IDCANCEL}) {
    HWND candidate = GetDlgItem(dialog, control);
    if (IsWindowEnabled(candidate)) {
      SendMessageW(dialog, WM_NEXTDLGCTL, reinterpret_cast<WPARAM>(candidate),
                   TRUE);
      return;
    }
  }
}

std::wstring MenuTitle() { return app_info::kMenuTitle; }

void SetText(HWND dialog, int control, const std::wstring& text) {
  SetDlgItemTextW(dialog, control, text.c_str());
}

// Shows whether the command is in the menu and enables the matching buttons.
void RefreshStatus(HWND dialog, const WindowState& state) {
  const bool can_add = package_registration::CanAdd();
  const package_registration::Status status = package_registration::Query();
  std::wstring text;
  bool add_enabled = false;
  bool remove_enabled = false;
  switch (status.state) {
    case State::kNotAdded:
      text = MenuTitle() +
             L" is not in the File Explorer context menu yet. Add it to copy "
             L"file and folder paths with forward slashes.";
      add_enabled = can_add;
      break;
    case State::kAdded:
      text = MenuTitle() +
             L" is in the File Explorer context menu. Right-click a file, a "
             L"folder, or the background of a folder to use it.";
      remove_enabled = true;
      break;
    case State::kAddedElsewhere:
      text = MenuTitle() + L" is in the context menu, but it runs from " +
             (status.location.empty() ? L"another folder"
                                      : status.location) +
             L". Add it again to run it from this folder.";
      add_enabled = can_add;
      remove_enabled = true;
      break;
    case State::kNeedsUpdate:
      text = MenuTitle() + L" is in the context menu from version " +
             status.version + L". Add it again to " +
             (status.newer ? L"use version " : L"update it to version ") +
             app_info::kVersion + (status.newer ? L" instead." : L".");
      add_enabled = can_add;
      remove_enabled = true;
      break;
  }
  if (!can_add && status.state != State::kAdded) {
    text += L" This folder is missing " +
            std::wstring(app_info::kPackageFile) +
            L", so download Plain Path Copy again to add it.";
  }
  if (!state.last_error.empty()) {
    text = state.last_error;
  }
  SetText(dialog, IDC_MENU_STATUS, text);
  EnableWindow(GetDlgItem(dialog, IDC_ADD), add_enabled && !state.busy);
  EnableWindow(GetDlgItem(dialog, IDC_REMOVE), remove_enabled && !state.busy);
  KeepFocusUsable(dialog);
}

int RadioFor(QuoteMode mode) {
  switch (mode) {
    case QuoteMode::kAlways:
      return IDC_QUOTE_ALWAYS;
    case QuoteMode::kNever:
      return IDC_QUOTE_NEVER;
    case QuoteMode::kWhenNeeded:
      break;
  }
  return IDC_QUOTE_WHEN_NEEDED;
}

// Shows how two sample paths are copied with the chosen quotation marks.
void RefreshExample(HWND dialog, QuoteMode mode) {
  SetText(dialog, IDC_QUOTE_EXAMPLE,
          L"Copies " + path_format::FormatPaths({kPlainSample}, mode) +
              L" and " + path_format::FormatPaths({kSpacedSample}, mode));
}

void SaveQuoteMode(HWND dialog, QuoteMode mode) {
  if (settings::WriteQuoteMode(mode) != ERROR_SUCCESS) {
    MessageBoxW(dialog, L"The setting couldn't be saved.", app_info::kName,
                MB_OK | MB_ICONWARNING);
  }
  RefreshExample(dialog, mode);
}

void ShowFeedbackResult(HWND dialog, bool opened, const wchar_t* failure) {
  HWND error = GetDlgItem(dialog, IDC_FEEDBACK_ERROR);
  if (opened) {
    ShowWindow(error, SW_HIDE);
    return;
  }
  SetWindowTextW(error, failure);
  ShowWindow(error, SW_SHOW);
}

// Adds or removes the package on a worker thread; the dialog stays
// responsive and hears back through kOperationFinished.
void Start(HWND dialog, WindowState& state, Operation operation) {
  state.busy = true;
  state.last_error.clear();
  std::wstring status;
  if (operation == Operation::kAdd) {
    status = L"Adding to the context menu...";
  } else {
    status =
        L"Removing from the context menu... Windows waits until File "
        L"Explorer stops using the command, which can take a few minutes.";
    if (RunningFromPackage()) {
      status += L" Plain Path Copy closes when Windows finishes.";
    }
  }
  SetText(dialog, IDC_MENU_STATUS, status);
  EnableWindow(GetDlgItem(dialog, IDC_ADD), FALSE);
  EnableWindow(GetDlgItem(dialog, IDC_REMOVE), FALSE);
  KeepFocusUsable(dialog);
  if (state.worker.joinable()) {
    state.worker.join();  // The previous operation has already finished.
  }
  state.worker = std::thread([dialog, operation] {
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    auto outcome = std::make_unique<OperationOutcome>();
    outcome->operation = operation;
    outcome->result = operation == Operation::kAdd
                          ? package_registration::Add()
                          : package_registration::Remove();
    if (PostMessageW(dialog, kOperationFinished, 0,
                     reinterpret_cast<LPARAM>(outcome.get()))) {
      outcome.release();
    }
    winrt::uninit_apartment();
  });
}

void Finish(HWND dialog, WindowState& state, OperationOutcome* raw) {
  const std::unique_ptr<OperationOutcome> outcome(raw);
  state.busy = false;
  if (state.worker.joinable()) {
    state.worker.join();
  }
  if (state.close_when_done) {
    EndDialog(dialog, IDCANCEL);
    return;
  }
  if (!outcome->result.succeeded) {
    state.last_error =
        (outcome->operation == Operation::kAdd
             ? L"Couldn't add the command to the context menu. "
             : L"Couldn't remove the command from the context menu. ") +
        outcome->result.message;
  }
  RefreshStatus(dialog, state);
}

void SetWindowIcons(HWND dialog) {
  HINSTANCE instance = GetModuleHandleW(nullptr);
  HICON large = nullptr;
  HICON small_icon = nullptr;
  LoadIconMetric(instance, MAKEINTRESOURCEW(IDI_APP_ICON), LIM_LARGE, &large);
  LoadIconMetric(instance, MAKEINTRESOURCEW(IDI_APP_ICON), LIM_SMALL,
                 &small_icon);
  SendMessageW(dialog, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(large));
  SendMessageW(dialog, WM_SETICON, ICON_SMALL,
               reinterpret_cast<LPARAM>(small_icon));
}

void DestroyWindowIcons(HWND dialog) {
  for (WPARAM which : {ICON_BIG, ICON_SMALL}) {
    if (auto icon = reinterpret_cast<HICON>(
            SendMessageW(dialog, WM_GETICON, which, 0))) {
      DestroyIcon(icon);
    }
  }
}

void Initialize(HWND dialog, WindowState& state) {
  SetWindowIcons(dialog);
  SetText(dialog, IDC_FEEDBACK_INTRO, kFeedbackIntro);
  SetText(dialog, IDC_VERSION, std::wstring(L"Version ") + app_info::kVersion);
  const QuoteMode mode = settings::ReadQuoteMode();
  CheckRadioButton(dialog, IDC_QUOTE_WHEN_NEEDED, IDC_QUOTE_NEVER,
                   RadioFor(mode));
  RefreshExample(dialog, mode);
  RefreshStatus(dialog, state);
}

INT_PTR CALLBACK DialogProc(HWND dialog, UINT message, WPARAM wparam,
                            LPARAM lparam) {
  auto* state =
      reinterpret_cast<WindowState*>(GetWindowLongPtrW(dialog, DWLP_USER));
  switch (message) {
    case WM_INITDIALOG:
      state = reinterpret_cast<WindowState*>(lparam);
      SetWindowLongPtrW(dialog, DWLP_USER, lparam);
      Initialize(dialog, *state);
      return TRUE;

    case WM_ACTIVATE:
      // Pick up changes made elsewhere, such as removing the app in Settings.
      if (LOWORD(wparam) != WA_INACTIVE && state != nullptr && !state->busy) {
        RefreshStatus(dialog, *state);
      }
      return FALSE;

    case WM_CTLCOLORSTATIC:
      if (reinterpret_cast<HWND>(lparam) ==
          GetDlgItem(dialog, IDC_FEEDBACK_ERROR)) {
        auto dc = reinterpret_cast<HDC>(wparam);
        HIGHCONTRASTW contrast{sizeof(contrast)};
        const bool high_contrast =
            SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast),
                                  &contrast, 0) &&
            (contrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
        SetTextColor(dc, high_contrast ? GetSysColor(COLOR_WINDOWTEXT)
                                       : kErrorColor);
        SetBkMode(dc, TRANSPARENT);
        return reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_BTNFACE));
      }
      return FALSE;

    case WM_DESTROY:
      DestroyWindowIcons(dialog);
      return FALSE;

    case kOperationFinished:
      Finish(dialog, *state, reinterpret_cast<OperationOutcome*>(lparam));
      return TRUE;

    case WM_COMMAND:
      switch (LOWORD(wparam)) {
        case IDC_ADD:
          Start(dialog, *state, Operation::kAdd);
          return TRUE;
        case IDC_REMOVE:
          Start(dialog, *state, Operation::kRemove);
          return TRUE;
        case IDC_QUOTE_WHEN_NEEDED:
          SaveQuoteMode(dialog, QuoteMode::kWhenNeeded);
          return TRUE;
        case IDC_QUOTE_ALWAYS:
          SaveQuoteMode(dialog, QuoteMode::kAlways);
          return TRUE;
        case IDC_QUOTE_NEVER:
          SaveQuoteMode(dialog, QuoteMode::kNever);
          return TRUE;
        case IDC_REPORT_BUG:
          ShowFeedbackResult(
              dialog, feedback::OpenEmail(dialog, feedback::Kind::kBugReport),
              kEmailFailure);
          return TRUE;
        case IDC_REQUEST_FEATURE:
          ShowFeedbackResult(
              dialog,
              feedback::OpenEmail(dialog, feedback::Kind::kFeatureRequest),
              kEmailFailure);
          return TRUE;
        case IDC_CONTACT_FORM:
          ShowFeedbackResult(dialog, feedback::OpenContactForm(dialog),
                             kContactFailure);
          return TRUE;
        case IDCANCEL:
          if (state->busy) {
            // Closing now would end the process in the middle of Windows'
            // work, so close as soon as it finishes instead.
            state->close_when_done = true;
            SetText(dialog, IDC_MENU_STATUS,
                    L"Plain Path Copy will close when Windows finishes.");
            return TRUE;
          }
          EndDialog(dialog, IDCANCEL);
          return TRUE;
        default:
          return FALSE;
      }

    default:
      return FALSE;
  }
}

}  // namespace

void Run(HINSTANCE instance) {
  WindowState state;
  DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_MAIN), nullptr, DialogProc,
                  reinterpret_cast<LPARAM>(&state));
  if (state.worker.joinable()) {
    state.worker.join();
  }
}

}  // namespace settings_window
