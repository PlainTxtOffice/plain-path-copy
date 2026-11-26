<#
.SYNOPSIS
Loads the Visual Studio x64 build environment into the current session.

.DESCRIPTION
Dot-source this script from the other build scripts. It finds the newest
Visual Studio installation with the C++ tools, then enters its developer shell
so cl, cmake, ninja, and the Windows SDK tools are on PATH. It does nothing
when the environment is already loaded.
#>
$ErrorActionPreference = 'Stop'

if ($env:VSCMD_ARG_TGT_ARCH -eq 'x64' -and (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    return
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw 'Visual Studio was not found. Install Visual Studio with the "Desktop development with C++" workload.'
}
$installPath = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
if (-not $installPath) {
    throw 'No Visual Studio installation has the C++ x64 build tools.'
}

# The developer shell runs vswhere by name, so put its folder on PATH.
$env:PATH = "$(Split-Path -Parent $vswhere);$env:PATH"
Import-Module (Join-Path $installPath 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $installPath -SkipAutomaticLocation `
    -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
