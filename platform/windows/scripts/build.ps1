<#
.SYNOPSIS
Builds Plain Path Copy and optionally runs its tests.

.DESCRIPTION
Configures the CMake project on first use, then builds every target. The
DLL, the settings app, AppxManifest.xml, and the package logos land in
build/x64/layout/<Configuration>/, the folder the development package points
at.

.PARAMETER Configuration
Debug or Release. Defaults to Debug.

.PARAMETER Test
Runs the unit and integration tests after building.

.EXAMPLE
./scripts/build.ps1 -Configuration Release -Test
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [switch]$Test
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'enter-dev-shell.ps1')

Push-Location $projectRoot
try {
    if (-not (Test-Path -LiteralPath 'build/x64/CMakeCache.txt')) {
        cmake --preset x64
        if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
    }
    cmake --build --preset $Configuration.ToLowerInvariant()
    if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
    if ($Test) {
        ctest --preset $Configuration.ToLowerInvariant()
        if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }
    }
}
finally {
    Pop-Location
}
