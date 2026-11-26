# Windows application

Plain Path Copy for Windows is a C++ project built with CMake and MSVC. Run
these commands from `platform/windows/` in PowerShell:

```powershell
./scripts/build.ps1 -Configuration Debug -Test
./scripts/make-icons.ps1
./scripts/build-release.ps1 -Sign
```

`build.ps1` builds everything and runs the tests, `make-icons.ps1`
regenerates the icon files from the repository's master icon, and
`build-release.ps1` makes the signed portable zip.

Building needs Visual Studio 2022 or later with the "Desktop development with
C++" workload; nothing else is downloaded. To try a development build in File
Explorer, turn on Developer Mode, run `plain-path-copy.exe` from
`build/x64/layout/Debug/`, and select **Add to Context Menu**.

See the [repository README](../../README.md) for project-wide documentation.
