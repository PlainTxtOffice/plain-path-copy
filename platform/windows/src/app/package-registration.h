// Adds and removes the sparse package that puts the command in the menu.
#pragma once

#include <string>

namespace package_registration {

enum class State {
  kNotAdded,        // No package is registered for this user.
  kAdded,           // Registered, pointing at this app's folder.
  kAddedElsewhere,  // Registered, pointing at another folder.
  kNeedsUpdate,     // Registered from this folder, but another version.
};

struct Status {
  State state = State::kNotAdded;
  std::wstring location;  // The registered external location, if any.
  std::wstring version;   // The registered package version, if any.
  bool newer = false;     // The registered version is newer than this app.
};

struct Result {
  bool succeeded = false;
  std::wstring message;  // Windows' explanation when it failed.
};

// Returns the folder that holds this executable, without a trailing slash.
std::wstring AppDirectory();

// Returns true when this folder holds something Add can register: the
// signed plain-path-copy.msix of a release, or the AppxManifest.xml of a
// development build.
bool CanAdd();

// Reports whether the package is registered for the current user.
Status Query();

// Registers the package with this app's folder as its external location.
// An older registration from this folder is updated in place; one from
// another folder, or a newer one, is removed first. Blocks until Windows
// finishes; call it from a worker thread.
Result Add();

// Removes the package for the current user. Blocks; call it from a worker
// thread.
Result Remove();

}  // namespace package_registration
