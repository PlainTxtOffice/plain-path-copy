<#
.SYNOPSIS
Builds the portable release zip of Plain Path Copy.

.DESCRIPTION
Builds and tests the Release configuration, packs the sparse package with
makeappx, and zips the app as build/release/plain-path-copy-<version>-portable.zip.

The zip holds plain-path-copy.exe, plain-path-copy-shell.dll,
plain-path-copy.msix, and the package logos. They are staged in
build/release/payload/, which scripts/advanced-installer/build-installer.ps1
packages into the setup program. Windows installs only a signed package, so a
release for other people must be built with -Sign. Without it the zip is
useful only for checking the layout.

.PARAMETER Sign
Signs the DLL, the executable, and the package with the Plain Text Office
certificate named by the WINDOWS_CERTIFICATE_SHA1 environment variable. See
dev-docs/build_instructions/code_signing/SETUP.md. Every signature uses one
SSL.com eSigner signing.

.EXAMPLE
./scripts/build-release.ps1 -Sign
#>
[CmdletBinding()]
param(
    [switch]$Sign
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'enter-dev-shell.ps1')

function Invoke-Checked {
    param([string]$Description, [scriptblock]$Command)
    & $Command
    if ($LASTEXITCODE -ne 0) { throw "$Description failed." }
}

function Set-Signature {
    param([string[]]$Paths)
    if (-not $env:WINDOWS_CERTIFICATE_SHA1) {
        throw 'WINDOWS_CERTIFICATE_SHA1 is not set. See dev-docs/build_instructions/code_signing/SETUP.md.'
    }
    Invoke-Checked 'Signing' {
        signtool sign /sha1 $env:WINDOWS_CERTIFICATE_SHA1 /fd sha256 `
            /tr http://ts.ssl.com /td sha256 @Paths
    }
}

Push-Location $projectRoot
try {
    & (Join-Path $PSScriptRoot 'build.ps1') -Configuration Release -Test

    $cmake = Get-Content -Raw -LiteralPath 'CMakeLists.txt'
    if ($cmake -notmatch 'project\(plain-path-copy VERSION (\d+\.\d+\.\d+)') {
        throw 'Could not read the version from CMakeLists.txt.'
    }
    $version = $Matches[1]
    $layout = Join-Path $projectRoot 'build/x64/layout/Release'
    $releaseRoot = Join-Path $projectRoot 'build/release'
    $name = "plain-path-copy-$version-portable"
    # A version-independent folder, so the installer project can keep
    # synchronizing the same path from release to release.
    $stage = Join-Path $releaseRoot 'payload'
    $packageLayout = Join-Path $releaseRoot 'package'
    $zip = Join-Path $releaseRoot "$name.zip"

    foreach ($path in @($stage, $packageLayout, $zip)) {
        if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Recurse -Force }
    }
    New-Item -ItemType Directory -Force -Path (Join-Path $stage 'assets') | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $packageLayout 'assets') | Out-Null

    Copy-Item -LiteralPath (Join-Path $layout 'plain-path-copy.exe') -Destination $stage
    Copy-Item -LiteralPath (Join-Path $layout 'plain-path-copy-shell.dll') -Destination $stage
    Copy-Item -Path (Join-Path $layout 'assets/*.png') -Destination (Join-Path $stage 'assets')
    Copy-Item -LiteralPath (Join-Path $layout 'AppxManifest.xml') -Destination $packageLayout
    Copy-Item -Path (Join-Path $layout 'assets/*.png') -Destination (Join-Path $packageLayout 'assets')

    if ($Sign) {
        Set-Signature @((Join-Path $stage 'plain-path-copy.exe'), (Join-Path $stage 'plain-path-copy-shell.dll'))
    }

    $msix = Join-Path $stage 'plain-path-copy.msix'
    Invoke-Checked 'makeappx' { makeappx pack /o /nv /d $packageLayout /p $msix }
    if ($Sign) {
        Set-Signature @($msix)
    }
    else {
        Write-Warning 'The package is unsigned; Windows will refuse to add it. Use -Sign for a release.'
    }

    Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip
    Write-Output "Release zip: $zip"
}
finally {
    Pop-Location
}
