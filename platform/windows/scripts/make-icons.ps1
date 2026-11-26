<#
.SYNOPSIS
Regenerates the Windows icon and package logos from the master icon.

.DESCRIPTION
Builds the make-icons tool, then renders the repository's master icon,
assets/icons/icon.svg, into:

- assets/plain-path-copy.ico, embedded in the DLL and the settings app.
- assets/package/*.png, the logos the sparse package manifest names.

Run it whenever the master icon changes, and commit the results so builds do
not need to run it.
#>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$repositoryRoot = Split-Path -Parent (Split-Path -Parent $projectRoot)
. (Join-Path $PSScriptRoot 'enter-dev-shell.ps1')

Push-Location $projectRoot
try {
    if (-not (Test-Path -LiteralPath 'build/x64/CMakeCache.txt')) {
        cmake --preset x64
        if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
    }
    cmake --build --preset release --target make-icons
    if ($LASTEXITCODE -ne 0) { throw 'Building make-icons failed.' }

    $master = Join-Path $repositoryRoot 'assets/icons/icon.svg'
    $package = Join-Path $projectRoot 'assets/package'
    New-Item -ItemType Directory -Force -Path $package | Out-Null
    & (Join-Path $projectRoot 'build/x64/tools/Release/make-icons.exe') $master `
        --ico (Join-Path $projectRoot 'assets/plain-path-copy.ico') `
        --png 44 (Join-Path $package 'square-44.png') `
        --png 150 (Join-Path $package 'square-150.png') `
        --png 50 (Join-Path $package 'store-logo.png')
    if ($LASTEXITCODE -ne 0) { throw 'Rendering the icons failed.' }
}
finally {
    Pop-Location
}
