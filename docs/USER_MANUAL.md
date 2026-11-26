---
last_updated: "2026-11-25"
version: "6.0.0"
product_versions:
  windows: "1.0.0"
generated_from: "dev-docs/specs/USER_MANUAL_SPEC.instructions.md"
---

# User Manual

## Overview

Plain Path Copy adds **Copy as path (/)** to the File Explorer context menu.
It copies the paths of the selected files and folders with forward slashes,
such as `H:/documents/work/notes.txt`, instead of the backslashes Windows'
own **Copy as path** uses. That is the form most programming tools, shells,
configuration files, and web addresses expect.

It is for anyone who pastes Windows paths into code, terminals, or documents
and is tired of replacing backslashes by hand. Windows' own **Copy as path**
stays in the menu unchanged.

Plain Path Copy runs on Windows. It has a context menu command and a small
settings window.
<!-- Verified from: platform/windows/CMakeLists.txt [APP_MENU_TITLE] -->
<!-- Verified from: platform/windows/src/core/path-format.cpp -->

## Installation and Setup

Plain Path Copy needs Windows 11 on an x64 PC. It comes two ways, and neither
needs administrator rights:

| Download | What it does |
| --- | --- |
| [plain-path-copy-setup.exe](https://github.com/PlainTxtOffice/plain-path-copy/releases/latest/download/plain-path-copy-setup.exe) | Installs Plain Path Copy for your account and adds the command for you |
| [plain-path-copy-portable.zip](https://github.com/PlainTxtOffice/plain-path-copy/releases/latest/download/plain-path-copy-portable.zip) | Runs from a folder you choose; you add the command yourself |
<!-- Verified from: platform/windows/package/AppxManifest.xml.in [ProcessorArchitecture] -->
<!-- Verified from: https://github.com/PlainTxtOffice/plain-path-copy/releases/tag/v1.0.0 -->

### Install with the Setup Program

1. Download `plain-path-copy-setup.exe` and open it.
1. Select **Next**, then **Next** again to keep the suggested folder, then
   **Install**.
1. Select **Finish**.

Setup installs Plain Path Copy into
`C:/Users/<you>/AppData/Local/Programs/plain-text-office/plain-path-copy` and
adds **Copy as path (/)** to the File Explorer context menu.
<!-- Verified from: platform/windows/scripts/advanced-installer/build-installer.ps1 [Initialize-Project] -->
<!-- Verified from: an interactive install of plain-path-copy-1.0.0-setup.exe [Welcome, Select Installation Folder, Ready to Install, Completing pages] -->

### Add the Command to the Context Menu

These steps are for the portable version. After the setup program finishes,
the command is already in the menu.

1. Extract `plain-path-copy-portable.zip` to the folder where Plain Path Copy
   will stay, such as `C:/Users/<you>/Programs/plain-path-copy`. The menu
   entry runs the program from this folder, so moving the folder later
   removes the command from the menu until you add it again.
1. Open `plain-path-copy.exe` in that folder.
1. Select **Add to Context Menu**.

The status changes to say that **Copy as path (/)** is in the File Explorer
context menu. Plain Path Copy also appears in the Start menu, so you can open
its settings from there.
<!-- Verified from: platform/windows/src/app/settings-window.cpp [RefreshStatus] -->
<!-- Verified from: platform/windows/src/app/package-registration.cpp [Add] -->
<!-- Verified from: Get-StartApps on a development build [Plain Path Copy] -->
<!-- Verified from: platform/windows/scripts/advanced-installer/build-installer.ps1 [Initialize-Project] -->

### Update to a New Version

If you installed Plain Path Copy with its setup program, run the new setup
program. It replaces the old version and keeps the command in the menu.

For the portable version:

1. Close Plain Path Copy if it is open.
1. Extract the new version over the old one, into the same folder.
1. Open `plain-path-copy.exe` in that folder. The status says which version
   is in the context menu.
1. Select **Add to Context Menu** to update it.
<!-- Verified from: platform/windows/src/app/settings-window.cpp [RefreshStatus kNeedsUpdate] -->
<!-- Verified from: platform/windows/src/app/package-registration.cpp [Add] -->

### Remove Plain Path Copy

If you installed Plain Path Copy with its setup program, uninstall it from
**Settings** > **Apps** > **Installed apps**. That removes the command and the
program's files.

For the portable version:

1. Open Plain Path Copy and select **Remove from Context Menu**.
1. Wait for the status to say the command is not in the context menu.
   Windows waits until File Explorer stops using the command, which can take
   a few minutes.
1. Close Plain Path Copy and delete its folder.

Removing the command does not delete the folder.
<!-- Verified from: platform/windows/src/app/settings-window.cpp [Start] -->
<!-- Verified from: platform/windows/src/app/package-registration.cpp [Remove] -->

## Starting the Program

There is nothing to start for everyday use: once added, **Copy as path (/)**
is part of the File Explorer context menu.

To change settings or send feedback, open the settings window in either of
these ways:

- Open `plain-path-copy.exe` in the Plain Path Copy folder.
- Open **Plain Path Copy** from the Start menu, after the command has been
  added.

Close the window with **Close**, the window's close button, or **Esc**.
<!-- Verified from: platform/windows/src/app/main.cpp -->
<!-- Verified from: platform/windows/src/app/app.rc [IDD_MAIN] -->

## Getting Around

The settings window has three groups:

| Group | What it shows |
| --- | --- |
| **File Explorer context menu** | Whether the command is in the menu, with **Add to Context Menu** and **Remove from Context Menu** |
| **Quotation marks** | When copied paths are wrapped in double quotation marks, with an example |
| **Feedback** | **Report a Bug**, **Request a Feature**, and **Open Contact Form** |

The version number is at the bottom left.
<!-- Verified from: platform/windows/src/app/app.rc [IDD_MAIN] -->

## Everyday Tasks

### Copy a Path with Forward Slashes

1. In File Explorer, right-click a file or folder.
1. Select **Copy as path (/)**. Recent versions of Windows 11 group commands
   from other apps under **App extensions**, so open that submenu first if
   the command is not in the main menu.
1. Paste wherever you need the path.

Each backslash becomes a forward slash:

| Selected item | Copied text |
| --- | --- |
| `H:\documents\work\notes.txt` | `H:/documents/work/notes.txt` |
| `C:\` | `C:/` |
| `\\server\share\report.docx` | `//server/share/report.docx` |
| `C:\My Files\notes.txt` | `"C:/My Files/notes.txt"` |

The last path has a space, so it is quoted. **Quotation marks** under
[Settings and Preferences](#settings-and-preferences) changes this.
<!-- Verified from: platform/windows/src/core/path-format.cpp [ToForwardSlashes, ApplyQuoteMode] -->
<!-- Verified from: platform/windows/tests/path-format-tests.cpp -->
<!-- Verified from: owner's screenshot of the Windows 11 context menu [App extensions] -->

### Copy the Path of the Folder You Are In

Right-click an empty part of the folder, not a file, and select **Copy as
path (/)**. Plain Path Copy copies the path of the open folder.
<!-- Verified from: platform/windows/package/AppxManifest.xml.in [Directory\Background] -->
<!-- Verified from: platform/windows/src/shell/copy-path-command.cpp [Invoke] -->

### Copy Several Paths at Once

Select several files or folders, right-click one of them, and select **Copy
as path (/)**. Each path goes on its own line, in the order File Explorer
lists the selection, with no line break after the last one.
<!-- Verified from: platform/windows/src/core/path-format.h [kPathSeparator] -->
<!-- Verified from: platform/windows/tests/shell-extension-tests.cpp [CopiesSeveralItemsOnSeparateLines] -->

### Know What Cannot Be Copied

Some items in File Explorer have no path on disk, such as **This PC**, the
Recycle Bin, and files inside a `.zip` folder. Plain Path Copy skips them in
a mixed selection, and the command does not appear when nothing selected has
a path.
<!-- Verified from: platform/windows/src/shell/copy-path-command.cpp [GetState, AppendFileSystemPaths] -->

## Settings and Preferences

| Setting | Default | Accepted values | How to change it |
| --- | --- | --- | --- |
| **Quotation marks** | **Only when the path contains spaces** | **Only when the path contains spaces**, **Always**, **Never** | Select an option in the settings window |

- **Only when the path contains spaces** quotes a path that contains a space
  or other blank character and leaves the rest bare. Pasted into a terminal,
  every path stays one argument.
- **Always** quotes every path, like Windows' own **Copy as path**.
- **Never** copies every path bare.

The example under the options shows how two sample paths will be copied. A
change is saved as soon as you select it, applies the next time you use the
command, and stays until you change it again.
<!-- Verified from: platform/windows/src/core/path-format.h [QuoteMode, kDefaultQuoteMode] -->
<!-- Verified from: platform/windows/src/app/settings-window.cpp [SaveQuoteMode, RefreshExample] -->

## Importing and Exporting

Not Applicable.
<!-- Verified from: platform/windows/src -->

## Commands and Shortcuts

The context menu has one command, **Copy as path (/)**. In the settings
window, hold **Alt** and press the underlined letter of a control:

| Shortcut | What it does |
| --- | --- |
| **Alt+A** | **Add to Context Menu** |
| **Alt+R** | **Remove from Context Menu** |
| **Alt+S** | Selects **Only when the path contains spaces** |
| **Alt+W** | Selects **Always** |
| **Alt+N** | Selects **Never** |
| **Alt+B** | **Report a Bug** |
| **Alt+F** | **Request a Feature** |
| **Alt+C** | **Open Contact Form** |
| **Esc** | Closes the window |
<!-- Verified from: platform/windows/src/app/app.rc [IDD_MAIN] -->

## Your Data Files

Plain Path Copy keeps no files of its own. It stores the **Quotation marks**
setting in the Windows registry, under
`HKEY_CURRENT_USER/Software/plain-text-office/plain-path-copy`, as the
`quote-mode` value: `0` for **Only when the path contains spaces**, `1` for
**Always**, and `2` for **Never**. Change it in the settings window rather
than by hand.

The menu entry itself is registered with Windows and points at the Plain
Path Copy folder. It writes no logs.
<!-- Verified from: platform/windows/src/core/settings.h [kKeyPath, kQuoteModeValue] -->

## Troubleshooting

| Symptom | Likely Cause | Resolution |
| --- | --- | --- |
| **Copy as path (/)** is not in the menu | The command has not been added | Open Plain Path Copy and select **Add to Context Menu** |
| **Copy as path (/)** is not in the main menu, but it has been added | Windows lists it under **App extensions** | Open **App extensions** in the context menu |
| The command disappeared after moving the folder | The menu entry still points at the old folder | Open `plain-path-copy.exe` from the new folder and select **Add to Context Menu** |
| The status says the command runs from another folder | It was added from a different copy of Plain Path Copy | Select **Add to Context Menu** to use this copy instead |
| The status says the command is in the menu from an older version | A new version was extracted, but the menu entry was not updated | Select **Add to Context Menu** |
| The status says the folder is missing `plain-path-copy.msix` | The folder is incomplete | Download Plain Path Copy again and keep all its files together |
| The command does not appear for an item | The item has no path on disk, such as **This PC** | See [Know What Cannot Be Copied](#know-what-cannot-be-copied) |
| **Remove from Context Menu** seems stuck | Windows waits until File Explorer stops using the command | Wait a few minutes; the status updates when it finishes. If you select **Close** meanwhile, Plain Path Copy closes once Windows finishes |
| The folder cannot be deleted | File Explorer is still using the command | Remove the command first, wait for it to finish, then delete the folder |
| **Report a Bug** shows that no email app is set up | Windows has no email app for `mailto:` links | Select **Open Contact Form** instead |
<!-- Verified from: platform/windows/src/app/settings-window.cpp [RefreshStatus, kEmailFailure] -->
<!-- Verified from: platform/windows/src/app/package-registration.cpp [Add, Query] -->

## FAQ

### Does It Replace Windows' Copy as Path?

No. Windows' **Copy as path** stays in the menu, and **Copy as path (/)**
appears next to it.

### Can I Copy Paths without Quotation Marks?

Yes. Select **Never** under **Quotation marks**.

### Does It Change the Files I Copy?

No. It only reads the selected items' paths and puts text on the clipboard.

### Why Does the Drive Letter Stay?

The copied path still names a Windows location, such as `C:/work`, which
Windows programs, Git, and most tools accept. It is not converted to a WSL or
Unix path such as `/mnt/c/work`.
<!-- Verified from: platform/windows/src/core/path-format.cpp -->

## Feedback

Send bug reports and feature requests by email to
<support@plaintxtoffice.com> or through the contact page at
<https://plaintxtoffice.com/contact/>.

| Platform | How to open feedback |
| --- | --- |
| Windows | Open Plain Path Copy and use the **Feedback** group |

**Report a Bug** and **Request a Feature** open a draft in your email app,
addressed to Plain Text Office, with the Plain Path Copy version, Windows
version, and processor type filled in. **Open Contact Form** opens the
contact page in your browser. Nothing is sent unless you send it.
<!-- Verified from: platform/windows/src/core/feedback.h [kAddress, kContactUrl] -->
<!-- Verified from: platform/windows/src/core/feedback.cpp [SystemSummary, MailtoUrl] -->

## Glossary

| Term | Meaning |
| --- | --- |
| Context menu | The menu that opens when you right-click in File Explorer |
| Forward slash | The `/` character |
| Path | The text that names a file or folder's location, such as `C:/work/notes.txt` |
| UNC path | A path to a shared folder on another computer, starting with two slashes |
