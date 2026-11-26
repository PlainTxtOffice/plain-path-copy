#include "package-registration.h"

#include <windows.h>
#include <shlwapi.h>

#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Management.Deployment.h>
#include <winrt/Windows.Storage.h>

#include <optional>

#include "app-info.h"

namespace package_registration {
namespace {

using winrt::Windows::ApplicationModel::Package;
using winrt::Windows::Foundation::Uri;
using winrt::Windows::Management::Deployment::AddPackageOptions;
using winrt::Windows::Management::Deployment::DeploymentResult;
using winrt::Windows::Management::Deployment::PackageManager;
using winrt::Windows::Management::Deployment::RegisterPackageOptions;

constexpr wchar_t kDevelopmentManifest[] = L"AppxManifest.xml";

std::wstring Join(const std::wstring& directory, const wchar_t* name) {
  return directory + L"\\" + name;
}

bool FileExists(const std::wstring& path) {
  const DWORD attributes = GetFileAttributesW(path.c_str());
  return attributes != INVALID_FILE_ATTRIBUTES &&
         (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

// Returns a file: URI for a local path, escaping spaces and other
// characters the way Windows expects.
Uri FileUri(const std::wstring& path) {
  // Escaping can triple a character, as in %20, so leave room for that.
  std::wstring url(path.size() * 3 + 16, L'\0');
  DWORD length = static_cast<DWORD>(url.size());
  if (FAILED(UrlCreateFromPathW(path.c_str(), url.data(), &length, 0))) {
    winrt::throw_hresult(E_INVALIDARG);
  }
  url.resize(length);
  return Uri(url);
}

// Compares two folder paths, ignoring case and a trailing backslash.
bool SameFolder(std::wstring a, std::wstring b) {
  while (!a.empty() && a.back() == L'\\') a.pop_back();
  while (!b.empty() && b.back() == L'\\') b.pop_back();
  return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_EQUAL;
}

std::optional<Package> FindPackage() {
  PackageManager manager;
  for (const Package& package : manager.FindPackagesForUser(
           L"", app_info::kPackageName, app_info::kPublisher)) {
    return package;
  }
  return std::nullopt;
}

std::wstring PackageLocation(const Package& package) {
  try {
    if (const auto folder = package.EffectiveExternalLocation()) {
      return std::wstring(folder.Path());
    }
  } catch (const winrt::hresult_error&) {
    // Older Windows builds lack EffectiveExternalLocation.
  }
  return {};
}

// Waits for a deployment operation and turns its outcome into a Result.
template <typename Operation>
Result Finish(const Operation& operation) {
  Result result;
  try {
    operation.get();
    result.succeeded = true;
  } catch (const winrt::hresult_error& error) {
    try {
      const DeploymentResult details = operation.GetResults();
      result.message = std::wstring(details.ErrorText());
    } catch (const winrt::hresult_error&) {
    }
    if (result.message.empty()) {
      result.message = std::wstring(error.message());
    }
  }
  return result;
}

Result RemoveFullName(const std::wstring& full_name) {
  PackageManager manager;
  return Finish(manager.RemovePackageAsync(full_name));
}

// Compares the registered package's version with this app's: negative when
// the registered one is older, zero when equal, positive when newer.
int CompareWithAppVersion(const Package& package) {
  const auto version = package.Id().Version();
  const unsigned short registered[4] = {version.Major, version.Minor,
                                        version.Build, version.Revision};
  for (int part = 0; part < 4; ++part) {
    if (registered[part] != app_info::kPackageVersion[part]) {
      return registered[part] < app_info::kPackageVersion[part] ? -1 : 1;
    }
  }
  return 0;
}

std::wstring VersionText(const Package& package) {
  const auto version = package.Id().Version();
  return std::to_wstring(version.Major) + L"." +
         std::to_wstring(version.Minor) + L"." +
         std::to_wstring(version.Build);
}

Result Failure(const winrt::hresult_error& error) {
  return Result{false, std::wstring(error.message())};
}

}  // namespace

std::wstring AppDirectory() {
  std::wstring path(MAX_PATH, L'\0');
  for (;;) {
    const DWORD length = GetModuleFileNameW(nullptr, path.data(),
                                            static_cast<DWORD>(path.size()));
    if (length == 0) {
      return {};
    }
    if (length < path.size()) {
      path.resize(length);
      break;
    }
    path.resize(path.size() * 2);
  }
  path.resize(path.find_last_of(L'\\'));
  return path;
}

bool CanAdd() {
  const std::wstring directory = AppDirectory();
  return FileExists(Join(directory, app_info::kPackageFile)) ||
         FileExists(Join(directory, kDevelopmentManifest));
}

Status Query() {
  Status status;
  try {
    if (const std::optional<Package> package = FindPackage()) {
      status.location = PackageLocation(*package);
      status.version = VersionText(*package);
      if (!SameFolder(status.location, AppDirectory())) {
        status.state = State::kAddedElsewhere;
      } else if (const int comparison = CompareWithAppVersion(*package);
                 comparison != 0) {
        status.state = State::kNeedsUpdate;
        status.newer = comparison > 0;
      } else {
        status.state = State::kAdded;
      }
    }
  } catch (const winrt::hresult_error&) {
    status = Status{};
  }
  return status;
}

Result Add() {
  try {
    const std::wstring directory = AppDirectory();
    if (const std::optional<Package> existing = FindPackage()) {
      const bool same_folder =
          SameFolder(PackageLocation(*existing), directory);
      const int comparison = CompareWithAppVersion(*existing);
      if (same_folder && comparison == 0) {
        return Result{true, {}};
      }
      // A newer version from the same folder updates the package in place,
      // keeping the command in the menu if the update fails. Anything else
      // (another folder, or going back to an older version) needs the old
      // registration removed first.
      const bool update_in_place = same_folder && comparison < 0;
      if (!update_in_place) {
        Result removed =
            RemoveFullName(std::wstring(existing->Id().FullName()));
        if (!removed.succeeded) {
          return removed;
        }
      }
    }

    PackageManager manager;
    const std::wstring package_file = Join(directory, app_info::kPackageFile);
    if (FileExists(package_file)) {
      AddPackageOptions options;
      options.ExternalLocationUri(FileUri(directory));
      return Finish(manager.AddPackageByUriAsync(FileUri(package_file),
                                                 options));
    }
    const std::wstring manifest = Join(directory, kDevelopmentManifest);
    if (FileExists(manifest)) {
      // A development build registers its unsigned manifest in place, which
      // Windows allows only with Developer Mode turned on.
      RegisterPackageOptions options;
      options.ExternalLocationUri(FileUri(directory));
      options.DeveloperMode(true);
      return Finish(manager.RegisterPackageByUriAsync(FileUri(manifest),
                                                      options));
    }
    return Result{false, L"This folder has no " +
                             std::wstring(app_info::kPackageFile) + L"."};
  } catch (const winrt::hresult_error& error) {
    return Failure(error);
  }
}

Result Remove() {
  try {
    if (const std::optional<Package> existing = FindPackage()) {
      return RemoveFullName(std::wstring(existing->Id().FullName()));
    }
    return Result{true, {}};
  } catch (const winrt::hresult_error& error) {
    return Failure(error);
  }
}

}  // namespace package_registration
