# Advanced Installer Build

`build-installer.ps1` packages the signed release files into
`build/installer/plain-path-copy-<version>-setup.exe`. Run it from any folder
after the release payload exists:

```powershell
$env:ADVINST_COM = 'C:\Program Files (x86)\Caphyon\Advanced Installer 24.2.1\bin\x86\AdvancedInstaller.com'
./scripts/build-release.ps1 -Sign
./scripts/advanced-installer/build-installer.ps1
```

## What the Setup Does

- Installs per user, without administrator rights, into
  `%LOCALAPPDATA%\Programs\plain-text-office\plain-path-copy`. Windows
  registers sparse packages per user, so a per-machine install would add the
  command only for whoever ran setup.
- After installing the files, runs `plain-path-copy.exe --add-to-context-menu`
  to register the signed sparse package with the install folder as its
  external location. A failed registration rolls the install back.
- Before removing the files on uninstall, runs
  `plain-path-copy.exe --remove-from-context-menu`. An upgrade removes the old
  version first, so the old registration is gone before the new one is added.
- Adds no shortcut of its own: the sparse package already puts **Plain Path
  Copy** in the Start menu.

## Inputs and Outputs

| Item | Path, relative to `platform/windows/` |
| --- | --- |
| Payload | `build/release/payload/`, written by `scripts/build-release.ps1` |
| Project | `dev-docs/build_instructions/advanced_installer/plain-path-copy.aip` |
| Setup program | `build/installer/plain-path-copy-<version>-setup.exe` |
| Icon | `assets/plain-path-copy.ico` |

The script refuses a payload whose executable, DLL, or package is not validly
signed, because Windows will not register an unsigned package.

| Parameter | Default |
| --- | --- |
| `-ProjectRoot` | `platform/windows/` |
| `-SourceDirectory` | `build/release/payload` |
| `-PackagingDirectory` | `dev-docs/build_instructions/advanced_installer` |
| `-OutputDirectory` | `build/installer` |
| `-AdvancedInstaller` | `$env:ADVINST_COM` |

Relative overrides resolve against `-ProjectRoot`, not the current folder.

## The Persistent Project

The first run creates the AIP and never overwrites it afterwards, because it
holds the installer's UpgradeCode. The command line has no commands for custom
actions or per-build property values, so the script adds those rows to the
new AIP itself: the per-user `ALLUSERS` value and the three custom actions.
Later runs only check that they are still there, then refresh the payload, set
the version, and rebuild.

Signing is set once on Advanced Installer's **Digital Signature** page with
the Plain Text Office certificate from the Current User store and the
`http://ts.ssl.com` timestamp service. Advanced Installer then signs the
cabinet, the MSI, and the setup program on every build, which uses three
SSL.com signings in addition to the three for the payload.

## Testing a Build

Install silently and check the log:

```powershell
./build/installer/plain-path-copy-1.0.0-setup.exe /exenoui /qn /L*v install.log
Get-AppxPackage PlainTextOffice.PlainPathCopy
```

Uninstall from **Settings** > **Apps** > **Installed apps**, or with
`msiexec /x <ProductCode> /qn`, taking the product code from the AIP.
