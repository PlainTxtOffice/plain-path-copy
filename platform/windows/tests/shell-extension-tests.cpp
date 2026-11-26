// Integration tests for the shell extension DLL.
//
// The tests load the built DLL and create the command through its exported
// class factory, the way the COM surrogate does, so no package registration
// is needed. They change the user's quote-mode setting and clipboard text
// and put both back afterwards.

#include <windows.h>
#include <shellapi.h>
#include <shlobj_core.h>
#include <shobjidl_core.h>
#include <wrl/client.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "app-info.h"
#include "clipboard.h"
#include "settings.h"
#include "test-support.h"

using Microsoft::WRL::ComPtr;
using path_format::QuoteMode;

namespace {

constexpr wchar_t kThisPcParsingName[] =
    L"::{20D04FE0-3AEA-1069-A2D8-08002B30309D}";
constexpr wchar_t kUntouchedClipboard[] = L"clipboard before invoke";
constexpr wchar_t kPublisherKeyPath[] = L"Software\\plain-text-office";

bool KeyExists(const wchar_t* path) {
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, path, 0, KEY_READ, &key) !=
      ERROR_SUCCESS) {
    return false;
  }
  RegCloseKey(key);
  return true;
}

using GetClassObjectFunction = HRESULT(STDAPICALLTYPE*)(REFCLSID, REFIID,
                                                         void**);

// Puts the user's quote-mode value back as it was.
class QuoteModeRestorer {
 public:
  QuoteModeRestorer() {
    DWORD value = 0;
    DWORD size = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER, settings::kKeyPath,
                     settings::kQuoteModeValue, RRF_RT_REG_DWORD, nullptr,
                     &value, &size) == ERROR_SUCCESS) {
      saved_ = value;
    }
    key_existed_ = KeyExists(settings::kKeyPath);
    publisher_key_existed_ = KeyExists(kPublisherKeyPath);
  }
  ~QuoteModeRestorer() {
    if (saved_) {
      const DWORD value = *saved_;
      RegSetKeyValueW(HKEY_CURRENT_USER, settings::kKeyPath,
                      settings::kQuoteModeValue, REG_DWORD, &value,
                      sizeof(value));
    } else if (key_existed_) {
      RegDeleteKeyValueW(HKEY_CURRENT_USER, settings::kKeyPath,
                         settings::kQuoteModeValue);
    } else {
      RegDeleteTreeW(HKEY_CURRENT_USER, settings::kKeyPath);
      RegDeleteKeyW(HKEY_CURRENT_USER, settings::kKeyPath);
      if (!publisher_key_existed_) {
        // Fails, as it should, if anything else now lives under the key.
        RegDeleteKeyW(HKEY_CURRENT_USER, kPublisherKeyPath);
      }
    }
  }
  QuoteModeRestorer(const QuoteModeRestorer&) = delete;
  QuoteModeRestorer& operator=(const QuoteModeRestorer&) = delete;

 private:
  std::optional<DWORD> saved_;
  bool key_existed_ = false;
  bool publisher_key_existed_ = false;
};

// Puts the user's clipboard back as it was. Every format stored in global
// memory is copied, which covers text, files, and images (as CF_DIB);
// formats held as GDI handles are rebuilt by Windows from those.
class ClipboardRestorer {
 public:
  ClipboardRestorer() {
    if (!OpenClipboard(nullptr)) {
      return;
    }
    for (UINT format = EnumClipboardFormats(0); format != 0;
         format = EnumClipboardFormats(format)) {
      if (IsGdiFormat(format)) {
        continue;
      }
      HANDLE data = GetClipboardData(format);
      const void* source = data == nullptr ? nullptr : GlobalLock(data);
      if (source == nullptr) {
        continue;
      }
      const auto* bytes = static_cast<const char*>(source);
      saved_.push_back({format, std::string(bytes, GlobalSize(data))});
      GlobalUnlock(data);
    }
    CloseClipboard();
  }
  ~ClipboardRestorer() {
    if (!OpenClipboard(nullptr)) {
      return;
    }
    EmptyClipboard();
    for (const auto& [format, bytes] : saved_) {
      HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes.size());
      if (memory == nullptr) {
        continue;
      }
      if (void* target = GlobalLock(memory)) {
        std::memcpy(target, bytes.data(), bytes.size());
        GlobalUnlock(memory);
        if (SetClipboardData(format, memory) != nullptr) {
          continue;
        }
      }
      GlobalFree(memory);
    }
    CloseClipboard();
  }
  ClipboardRestorer(const ClipboardRestorer&) = delete;
  ClipboardRestorer& operator=(const ClipboardRestorer&) = delete;

 private:
  static bool IsGdiFormat(UINT format) {
    switch (format) {
      case CF_BITMAP:
      case CF_DSPBITMAP:
      case CF_METAFILEPICT:
      case CF_DSPMETAFILEPICT:
      case CF_ENHMETAFILE:
      case CF_DSPENHMETAFILE:
      case CF_PALETTE:
      case CF_OWNERDISPLAY:
        return true;
      default:
        return false;
    }
  }

  std::vector<std::pair<UINT, std::string>> saved_;
};

// A temporary folder with a file whose path contains a space.
class TempTree {
 public:
  TempTree() {
    root_ = std::filesystem::temp_directory_path() /
            (L"plain-path-copy-test-" + std::to_wstring(GetCurrentProcessId()));
    std::filesystem::create_directories(root_ / L"with space");
    // The shell reports long, correctly cased names; TEMP may be a short
    // 8.3 path such as C:\Users\JOHNSM~1\..., so compare against the
    // long form.
    std::wstring long_root(32768, L'\0');
    const DWORD length = GetLongPathNameW(
        root_.c_str(), long_root.data(), static_cast<DWORD>(long_root.size()));
    if (length > 0 && length < long_root.size()) {
      long_root.resize(length);
      root_ = long_root;
    }
    std::ofstream(root_ / L"plain.txt") << "x";
    std::ofstream(root_ / L"with space" / L"file one.txt") << "x";
  }
  ~TempTree() {
    std::error_code ignored;
    std::filesystem::remove_all(root_, ignored);
  }
  TempTree(const TempTree&) = delete;
  TempTree& operator=(const TempTree&) = delete;

  std::wstring Plain() const { return (root_ / L"plain.txt").wstring(); }
  std::wstring Spaced() const {
    return (root_ / L"with space" / L"file one.txt").wstring();
  }
  std::wstring Root() const { return root_.wstring(); }

 private:
  std::filesystem::path root_;
};

std::wstring Forward(std::wstring path) {
  for (wchar_t& c : path) {
    if (c == L'\\') {
      c = L'/';
    }
  }
  return path;
}

ComPtr<IShellItemArray> MakeItems(const std::vector<std::wstring>& names) {
  std::vector<PIDLIST_ABSOLUTE> pidls;
  for (const std::wstring& name : names) {
    PIDLIST_ABSOLUTE pidl = nullptr;
    if (SUCCEEDED(SHParseDisplayName(name.c_str(), nullptr, &pidl, 0,
                                     nullptr))) {
      pidls.push_back(pidl);
    }
  }
  ComPtr<IShellItemArray> items;
  if (pidls.size() == names.size()) {
    SHCreateShellItemArrayFromIDLists(
        static_cast<UINT>(pidls.size()),
        const_cast<PCIDLIST_ABSOLUTE_ARRAY>(pidls.data()), &items);
  }
  for (PIDLIST_ABSOLUTE pidl : pidls) {
    CoTaskMemFree(pidl);
  }
  return items;
}

ComPtr<IExplorerCommand> CreateCommand(HMODULE dll) {
  ComPtr<IExplorerCommand> command;
  auto get_class_object = reinterpret_cast<GetClassObjectFunction>(
      GetProcAddress(dll, "DllGetClassObject"));
  if (get_class_object == nullptr) {
    return command;
  }
  CLSID clsid{};
  if (FAILED(CLSIDFromString(app_info::kCommandClsid, &clsid))) {
    return command;
  }
  ComPtr<IClassFactory> factory;
  if (SUCCEEDED(get_class_object(clsid, IID_PPV_ARGS(&factory)))) {
    factory->CreateInstance(nullptr, IID_PPV_ARGS(&command));
  }
  return command;
}

std::wstring TakeString(LPWSTR text) {
  std::wstring result = text == nullptr ? L"" : text;
  CoTaskMemFree(text);
  return result;
}

HMODULE g_dll = nullptr;

// Invokes the command on the paths with the quote mode set, returning the
// clipboard text afterwards.
std::wstring InvokeWith(const std::vector<std::wstring>& paths,
                        QuoteMode mode) {
  settings::WriteQuoteMode(mode);
  clipboard::SetText(kUntouchedClipboard, nullptr);
  ComPtr<IExplorerCommand> command = CreateCommand(g_dll);
  ComPtr<IShellItemArray> items = MakeItems(paths);
  EXPECT_TRUE(command != nullptr && items != nullptr);
  if (command == nullptr || items == nullptr) {
    return {};
  }
  EXPECT_TRUE(SUCCEEDED(command->Invoke(items.Get(), nullptr)));
  return clipboard::GetText();
}

TempTree* g_tree = nullptr;

}  // namespace

TEST(CreatesCommandFromClassFactory) {
  EXPECT_TRUE(CreateCommand(g_dll) != nullptr);
}

TEST(TitleIsMenuLabel) {
  ComPtr<IExplorerCommand> command = CreateCommand(g_dll);
  LPWSTR title = nullptr;
  EXPECT_TRUE(SUCCEEDED(command->GetTitle(nullptr, &title)));
  EXPECT_EQ(TakeString(title), L"Copy as path (/)");
}

TEST(IconRefersToDllResource) {
  ComPtr<IExplorerCommand> command = CreateCommand(g_dll);
  LPWSTR icon = nullptr;
  EXPECT_TRUE(SUCCEEDED(command->GetIcon(nullptr, &icon)));
  const std::wstring reference = TakeString(icon);
  EXPECT_TRUE(reference.ends_with(L"plain-path-copy-shell.dll,-101"));
  HICON loaded = nullptr;
  const std::wstring file = reference.substr(0, reference.rfind(L','));
  EXPECT_TRUE(ExtractIconExW(file.c_str(), -101, nullptr, &loaded, 1) == 1);
  if (loaded != nullptr) {
    DestroyIcon(loaded);
  }
}

TEST(CanonicalNameIsClsid) {
  ComPtr<IExplorerCommand> command = CreateCommand(g_dll);
  GUID name{};
  CLSID clsid{};
  CLSIDFromString(app_info::kCommandClsid, &clsid);
  EXPECT_TRUE(SUCCEEDED(command->GetCanonicalName(&name)));
  EXPECT_TRUE(IsEqualGUID(name, clsid));
}

TEST(EnabledForFiles) {
  ComPtr<IExplorerCommand> command = CreateCommand(g_dll);
  ComPtr<IShellItemArray> items = MakeItems({g_tree->Plain()});
  EXPCMDSTATE state = ECS_DISABLED;
  EXPECT_TRUE(SUCCEEDED(command->GetState(items.Get(), FALSE, &state)));
  EXPECT_TRUE(state == ECS_ENABLED);
}

TEST(HiddenForItemsWithoutFileSystemPath) {
  ComPtr<IExplorerCommand> command = CreateCommand(g_dll);
  ComPtr<IShellItemArray> items = MakeItems({kThisPcParsingName});
  EXPECT_TRUE(items != nullptr);
  EXPCMDSTATE state = ECS_ENABLED;
  EXPECT_TRUE(SUCCEEDED(command->GetState(items.Get(), FALSE, &state)));
  EXPECT_TRUE(state == ECS_HIDDEN);
}

TEST(CopiesPlainPathWithoutQuotesByDefault) {
  EXPECT_EQ(InvokeWith({g_tree->Plain()}, QuoteMode::kWhenNeeded),
            Forward(g_tree->Plain()));
}

TEST(QuotesPathWithSpaceByDefault) {
  EXPECT_EQ(InvokeWith({g_tree->Spaced()}, QuoteMode::kWhenNeeded),
            L"\"" + Forward(g_tree->Spaced()) + L"\"");
}

TEST(AlwaysModeQuotesEveryPath) {
  EXPECT_EQ(InvokeWith({g_tree->Plain()}, QuoteMode::kAlways),
            L"\"" + Forward(g_tree->Plain()) + L"\"");
}

TEST(NeverModeLeavesSpacesUnquoted) {
  EXPECT_EQ(InvokeWith({g_tree->Spaced()}, QuoteMode::kNever),
            Forward(g_tree->Spaced()));
}

TEST(CopiesFolderPath) {
  EXPECT_EQ(InvokeWith({g_tree->Root()}, QuoteMode::kNever),
            Forward(g_tree->Root()));
}

TEST(CopiesSeveralItemsOnSeparateLines) {
  EXPECT_EQ(InvokeWith({g_tree->Plain(), g_tree->Spaced()},
                       QuoteMode::kWhenNeeded),
            Forward(g_tree->Plain()) + L"\r\n\"" + Forward(g_tree->Spaced()) +
                L"\"");
}

TEST(InvalidStoredModeFallsBackToDefault) {
  const DWORD invalid = 7;
  RegSetKeyValueW(HKEY_CURRENT_USER, settings::kKeyPath,
                  settings::kQuoteModeValue, REG_DWORD, &invalid,
                  sizeof(invalid));
  EXPECT_TRUE(settings::ReadQuoteMode() == QuoteMode::kWhenNeeded);
}

TEST(LeavesClipboardAloneWithoutFileSystemItems) {
  clipboard::SetText(kUntouchedClipboard, nullptr);
  ComPtr<IExplorerCommand> command = CreateCommand(g_dll);
  ComPtr<IShellItemArray> items = MakeItems({kThisPcParsingName});
  EXPECT_TRUE(SUCCEEDED(command->Invoke(items.Get(), nullptr)));
  EXPECT_TRUE(SUCCEEDED(command->Invoke(nullptr, nullptr)));
  EXPECT_EQ(clipboard::GetText(), kUntouchedClipboard);
}

TEST(DllCanUnloadAfterRelease) {
  using CanUnloadFunction = HRESULT(STDAPICALLTYPE*)();
  auto can_unload = reinterpret_cast<CanUnloadFunction>(
      GetProcAddress(g_dll, "DllCanUnloadNow"));
  EXPECT_TRUE(can_unload != nullptr && can_unload() == S_OK);
}

int main() {
  if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) {
    std::printf("COM failed to start\n");
    return 1;
  }
  int result = 1;
  // CMake writes the path with forward slashes; LoadLibrary needs
  // backslashes.
  std::wstring dll_path = SHELL_DLL_PATH;
  for (wchar_t& c : dll_path) {
    if (c == L'/') {
      c = L'\\';
    }
  }
  g_dll = LoadLibraryW(dll_path.c_str());
  if (g_dll == nullptr) {
    std::printf("Could not load the shell extension DLL\n");
  } else {
    QuoteModeRestorer quote_mode;
    ClipboardRestorer saved_clipboard;
    TempTree tree;
    g_tree = &tree;
    result = test_support::RunAll();
    g_tree = nullptr;
  }
  CoUninitialize();
  return result;
}
