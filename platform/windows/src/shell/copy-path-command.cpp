// The "Copy as path (/)" File Explorer command.
//
// Windows 11 shows a packaged IExplorerCommand in its modern context menu.
// The sparse package's manifest registers this class, and a dllhost.exe COM
// surrogate loads the DLL, so a crash here never takes down Explorer.

#include <windows.h>
#include <shlobj_core.h>
#include <shlwapi.h>
#include <shobjidl_core.h>
// WRL's module storage triggers an alignment-padding warning (C4324) in the
// SDK header itself.
#pragma warning(push)
#pragma warning(disable : 4324)
#include <wrl/client.h>
#include <wrl/implements.h>
#include <wrl/module.h>
#pragma warning(pop)

#include <string>
#include <vector>

#include "app-info.h"
#include "clipboard.h"
#include "path-format.h"
#include "settings.h"
#include "shell-resource.h"

using Microsoft::WRL::ClassicCom;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::InProc;
using Microsoft::WRL::Module;
using Microsoft::WRL::RuntimeClass;
using Microsoft::WRL::RuntimeClassFlags;

namespace {

HMODULE g_module = nullptr;

// Returns the full path of this DLL, for the menu icon reference.
std::wstring ModulePath() {
  std::wstring path(MAX_PATH, L'\0');
  for (;;) {
    const DWORD length = GetModuleFileNameW(
        g_module, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0) {
      return {};
    }
    if (length < path.size()) {
      path.resize(length);
      return path;
    }
    path.resize(path.size() * 2);
  }
}

// Appends the file system path of every item that has one. Items such as
// This PC or a file inside a .zip folder have none and are skipped.
void AppendFileSystemPaths(IShellItemArray* items,
                           std::vector<std::wstring>& paths) {
  DWORD count = 0;
  if (items == nullptr || FAILED(items->GetCount(&count))) {
    return;
  }
  for (DWORD index = 0; index < count; ++index) {
    ComPtr<IShellItem> item;
    if (FAILED(items->GetItemAt(index, &item))) {
      continue;
    }
    PWSTR path = nullptr;
    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
      paths.emplace_back(path);
      CoTaskMemFree(path);
    }
  }
}

// Returns the folder shown in the Explorer window that hosts the menu. The
// classic menu passes no items when the folder background is clicked, so
// the folder comes from the site instead.
ComPtr<IShellItemArray> FolderFromSite(IUnknown* site) {
  ComPtr<IShellItemArray> result;
  if (site == nullptr) {
    return result;
  }
  ComPtr<IShellBrowser> browser;
  if (FAILED(IUnknown_QueryService(site, SID_STopLevelBrowser,
                                   IID_PPV_ARGS(&browser)))) {
    return result;
  }
  ComPtr<IShellView> view;
  ComPtr<IFolderView> folder_view;
  ComPtr<IShellItem> folder;
  if (SUCCEEDED(browser->QueryActiveShellView(&view)) &&
      SUCCEEDED(view.As(&folder_view)) &&
      SUCCEEDED(folder_view->GetFolder(IID_PPV_ARGS(&folder)))) {
    SHCreateShellItemArrayFromShellItem(folder.Get(), IID_PPV_ARGS(&result));
  }
  return result;
}

class __declspec(uuid(APP_COMMAND_CLSID_STRING)) CopyPathCommand final
    : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IExplorerCommand,
                          IObjectWithSite> {
 public:
  // IExplorerCommand

  IFACEMETHODIMP GetTitle(IShellItemArray* /*items*/, LPWSTR* name) override {
    return SHStrDupW(app_info::kMenuTitle, name);
  }

  IFACEMETHODIMP GetIcon(IShellItemArray* /*items*/, LPWSTR* icon) override {
    *icon = nullptr;
    const std::wstring module = ModulePath();
    if (module.empty()) {
      return E_FAIL;
    }
    const std::wstring reference =
        module + L",-" + std::to_wstring(IDI_APP_ICON);
    return SHStrDupW(reference.c_str(), icon);
  }

  IFACEMETHODIMP GetToolTip(IShellItemArray* /*items*/,
                            LPWSTR* tool_tip) override {
    *tool_tip = nullptr;
    return E_NOTIMPL;
  }

  IFACEMETHODIMP GetCanonicalName(GUID* name) override {
    *name = __uuidof(CopyPathCommand);
    return S_OK;
  }

  // Hides the command when nothing selected has a file system path. An
  // empty selection is a folder background, which always has one.
  IFACEMETHODIMP GetState(IShellItemArray* items, BOOL /*ok_to_be_slow*/,
                          EXPCMDSTATE* state) override {
    *state = ECS_ENABLED;
    DWORD count = 0;
    if (items != nullptr && SUCCEEDED(items->GetCount(&count)) && count > 0) {
      SFGAOF attributes = 0;
      if (SUCCEEDED(items->GetAttributes(SIATTRIBFLAGS_OR, SFGAO_FILESYSTEM,
                                         &attributes)) &&
          (attributes & SFGAO_FILESYSTEM) == 0) {
        *state = ECS_HIDDEN;
      }
    }
    return S_OK;
  }

  IFACEMETHODIMP Invoke(IShellItemArray* items, IBindCtx* /*bind*/) override {
    std::vector<std::wstring> paths;
    AppendFileSystemPaths(items, paths);
    if (paths.empty()) {
      const ComPtr<IShellItemArray> folder = FolderFromSite(site_.Get());
      AppendFileSystemPaths(folder.Get(), paths);
    }
    if (paths.empty()) {
      return S_OK;
    }
    const std::wstring text =
        path_format::FormatPaths(paths, settings::ReadQuoteMode());
    HWND owner = nullptr;
    if (site_ != nullptr) {
      IUnknown_GetWindow(site_.Get(), &owner);
    }
    return clipboard::SetText(text, owner);
  }

  IFACEMETHODIMP GetFlags(EXPCMDFLAGS* flags) override {
    *flags = ECF_DEFAULT;
    return S_OK;
  }

  IFACEMETHODIMP EnumSubCommands(IEnumExplorerCommand** commands) override {
    *commands = nullptr;
    return E_NOTIMPL;
  }

  // IObjectWithSite

  IFACEMETHODIMP SetSite(IUnknown* site) override {
    site_ = site;
    return S_OK;
  }

  IFACEMETHODIMP GetSite(REFIID riid, void** site) override {
    *site = nullptr;
    return site_ == nullptr ? E_FAIL : site_.CopyTo(riid, site);
  }

 private:
  ComPtr<IUnknown> site_;
};

CoCreatableClass(CopyPathCommand)

}  // namespace

STDAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void** object) {
  return Module<InProc>::GetModule().GetClassObject(clsid, riid, object);
}

STDAPI DllCanUnloadNow() {
  return Module<InProc>::GetModule().Terminate() ? S_OK : S_FALSE;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID /*reserved*/) {
  if (reason == DLL_PROCESS_ATTACH) {
    g_module = module;
    DisableThreadLibraryCalls(module);
  }
  return TRUE;
}
