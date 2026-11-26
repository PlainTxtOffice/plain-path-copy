<#
.SYNOPSIS
Builds the Plain Path Copy setup program with Advanced Installer.

.DESCRIPTION
Packages the signed release files that scripts/build-release.ps1 staged in
build/release/payload/ into build/installer/plain-path-copy-<version>-setup.exe.

The setup installs per user, without administrator rights, into
%LOCALAPPDATA%\Programs\plain-text-office\plain-path-copy. After installing the
files it runs "plain-path-copy.exe --add-to-context-menu", which registers the
sparse package with that folder as its external location. Before removing
the files on uninstall it runs "--remove-from-context-menu".

On first use the script creates the persistent project,
dev-docs/build_instructions/advanced_installer/plain-path-copy.aip. It never
overwrites that file, because it holds the installer's UpgradeCode.

.PARAMETER ProjectRoot
The Windows project folder. Defaults to platform/windows/.

.PARAMETER SourceDirectory
The payload folder, relative to ProjectRoot. Defaults to build/release/payload.

.PARAMETER PackagingDirectory
The folder holding the AIP, relative to ProjectRoot.

.PARAMETER OutputDirectory
Where the setup program is written, relative to ProjectRoot. Defaults to
build/installer.

.PARAMETER AdvancedInstaller
Path to AdvancedInstaller.com. Defaults to the ADVINST_COM environment
variable.

.EXAMPLE
./scripts/build-release.ps1 -Sign
./scripts/advanced-installer/build-installer.ps1
#>
[CmdletBinding()]
param(
    [string]$ProjectRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
    [string]$SourceDirectory = 'build/release/payload',
    [string]$PackagingDirectory = 'dev-docs/build_instructions/advanced_installer',
    [string]$OutputDirectory = 'build/installer',
    [string]$AdvancedInstaller = $env:ADVINST_COM
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ProductName = 'Plain Path Copy'
$Manufacturer = 'Plain Text Office'
$PublisherSlug = 'plain-text-office'
$AppSlug = 'plain-path-copy'
$ExecutableName = 'plain-path-copy.exe'
$BuildName = 'DefaultBuild'
# The File table key Advanced Installer gives plain-path-copy.exe when it
# synchronizes the payload; the custom actions run that file.
$ExecutableFileKey = 'plainpathcopy.exe'
$RequiredPayload = @($ExecutableName, 'plain-path-copy-shell.dll', 'plain-path-copy.msix')

function Resolve-UnderRoot {
    param([string]$Root, [string]$Path)
    if ([IO.Path]::IsPathRooted($Path)) { return $Path }
    return Join-Path $Root $Path
}

function Resolve-AdvancedInstallerPath {
    param([string]$Candidate)
    if (-not $Candidate) {
        throw 'Set ADVINST_COM or pass -AdvancedInstaller with the path to AdvancedInstaller.com.'
    }
    $path = $Candidate.Trim().Trim('"')
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "AdvancedInstaller.com was not found at '$path'."
    }
    return $path
}

function Invoke-InstallerCli {
    param([string[]]$CliArguments)
    & $script:AdvancedInstallerPath @CliArguments
    if ($LASTEXITCODE -ne 0) {
        throw "Advanced Installer failed with exit code $LASTEXITCODE`: $($CliArguments -join ' ')"
    }
}

function Get-AppVersion {
    param([string]$Root)
    $cmake = Get-Content -Raw -LiteralPath (Join-Path $Root 'CMakeLists.txt')
    if ($cmake -notmatch 'project\(plain-path-copy VERSION (\d+)\.(\d+)\.(\d+)') {
        throw 'Could not read a three-part version from CMakeLists.txt.'
    }
    $major, $minor, $build = [int]$Matches[1], [int]$Matches[2], [int]$Matches[3]
    if ($major -gt 255 -or $minor -gt 255 -or $build -gt 65535) {
        throw "Version $major.$minor.$build exceeds Windows Installer limits (255.255.65535)."
    }
    return "$major.$minor.$build"
}

function Get-PeMachine {
    param([string]$Path)
    $stream = [IO.File]::OpenRead($Path)
    try {
        $reader = [IO.BinaryReader]::new($stream)
        if ($stream.Length -lt 64 -or $reader.ReadUInt16() -ne 0x5A4D) {
            throw "$Path is not a Windows executable."
        }
        $stream.Position = 0x3C
        $offset = $reader.ReadInt32()
        if ($offset -lt 64 -or $offset + 6 -gt $stream.Length) {
            throw "$Path has an invalid PE header offset."
        }
        $stream.Position = $offset
        if ($reader.ReadUInt32() -ne 0x00004550) {
            throw "$Path has no PE signature."
        }
        return $reader.ReadUInt16()
    }
    finally {
        $stream.Dispose()
    }
}

function Test-Payload {
    param([string]$Directory)
    if (-not (Test-Path -LiteralPath $Directory -PathType Container)) {
        throw "The payload folder '$Directory' does not exist. Run scripts/build-release.ps1 -Sign first."
    }
    foreach ($name in $RequiredPayload) {
        $file = Join-Path $Directory $name
        if (-not (Test-Path -LiteralPath $file -PathType Leaf)) {
            throw "The payload is missing $name. Run scripts/build-release.ps1 -Sign again."
        }
        $signature = Get-AuthenticodeSignature -LiteralPath $file
        if ($signature.Status -ne 'Valid') {
            throw "$name is not validly signed ($($signature.Status)). Build the payload with scripts/build-release.ps1 -Sign."
        }
    }
    $machine = Get-PeMachine (Join-Path $Directory $ExecutableName)
    if ($machine -ne 0x8664) {
        throw ('{0} is not an x64 executable (PE machine 0x{1:X4}).' -f $ExecutableName, $machine)
    }
}

# Adds rows to one component of the AIP. Advanced Installer has no
# command-line commands for custom actions or per-build property values, so
# the project is initialized with these rows once and then left to the
# application.
function Add-AipRows {
    param([string]$Path, [string]$Component, [string[]]$Rows)
    $text = [IO.File]::ReadAllText($Path)
    $pattern = '(<COMPONENT cid="' + [regex]::Escape($Component) + '">)'
    if ($text -notmatch $pattern) {
        throw "The AIP has no $Component component."
    }
    $insert = ($Rows | ForEach-Object { "`n    $_" }) -join ''
    $text = [regex]::Replace($text, $pattern, ('$1' + $insert), 1)
    [IO.File]::WriteAllText($Path, $text, [Text.UTF8Encoding]::new($false))
}

function Initialize-Project {
    param([string]$ProjectFile, [string]$Payload, [string]$IconFile)
    $temporary = Join-Path (Split-Path -Parent $ProjectFile) ("init-" + [guid]::NewGuid().ToString('N') + '.aip')
    try {
        Invoke-InstallerCli @('/newproject', $temporary, '-type', 'professional', '-lang', 'en')
        Invoke-InstallerCli @('/edit', $temporary, '/SetProperty', "ProductName=$ProductName")
        Invoke-InstallerCli @('/edit', $temporary, '/SetProperty', "Manufacturer=$Manufacturer")
        Invoke-InstallerCli @('/edit', $temporary, '/SetPackageType', 'x64', '-buildname', $BuildName)
        Invoke-InstallerCli @('/edit', $temporary, '/SetAppdir', '-buildname', $BuildName, '-path', "[LocalAppDataFolder]Programs\$PublisherSlug\$AppSlug")
        Invoke-InstallerCli @('/edit', $temporary, '/NewSync', 'APPDIR', $Payload, '-existingfiles', 'keep')
        Invoke-InstallerCli @('/edit', $temporary, '/RefreshSync', 'APPDIR')
        Invoke-InstallerCli @('/edit', $temporary, '/SetOutputType', 'ExeInside', '-buildname', $BuildName)
        Invoke-InstallerCli @('/edit', $temporary, '/SetOutputExeIcon', '-buildname', $BuildName, '-iconpath', $IconFile)
        Invoke-InstallerCli @('/edit', $temporary, '/SetIcon', '-icon', $IconFile)

        # Per-user only, as Advanced Installer's Install Parameters page
        # writes it: ALLUSERS empty for the build. Windows registers sparse
        # packages per user, so a per-machine install would add the command
        # only for the user who ran setup.
        $text = [IO.File]::ReadAllText($temporary)
        $text = $text.Replace('<ROW Property="ALLUSERS" Value="1"/>', "<ROW Property=`"ALLUSERS`" Value=`"1`" MultiBuildValue=`"${BuildName}:`"/>")
        [IO.File]::WriteAllText($temporary, $text, [Text.UTF8Encoding]::new($false))

        # Type 18 runs an installed EXE: +1024 deferred, +256 rollback, +64
        # ignore the exit code. Adding fails the install when it fails;
        # removing never blocks an uninstall.
        Add-AipRows $temporary 'caphyon.advinst.msicomp.MsiCustActComponent' @(
            "<ROW Action=`"AddContextMenuCommand`" Type=`"1042`" Source=`"$ExecutableFileKey`" Target=`"--add-to-context-menu`"/>",
            "<ROW Action=`"RollbackContextMenuCommand`" Type=`"1362`" Source=`"$ExecutableFileKey`" Target=`"--remove-from-context-menu`"/>",
            "<ROW Action=`"RemoveContextMenuCommand`" Type=`"1106`" Source=`"$ExecutableFileKey`" Target=`"--remove-from-context-menu`"/>"
        )
        # Add after the files are installed and remove before they are
        # removed, both between InstallInitialize (1500) and InstallFinalize
        # (6600).
        Add-AipRows $temporary 'caphyon.advinst.msicomp.MsiInstExSeqComponent' @(
            '<ROW Action="RollbackContextMenuCommand" Condition="NOT (REMOVE~=&quot;ALL&quot;)" Sequence="6397"/>',
            '<ROW Action="AddContextMenuCommand" Condition="NOT (REMOVE~=&quot;ALL&quot;)" Sequence="6398"/>',
            '<ROW Action="RemoveContextMenuCommand" Condition="REMOVE~=&quot;ALL&quot;" Sequence="3450"/>'
        )
        Test-Project $temporary
        Move-Item -LiteralPath $temporary -Destination $ProjectFile
    }
    finally {
        if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Force }
        # Advanced Installer keeps backups beside each project it edits.
        $backups = [IO.Path]::ChangeExtension($temporary, $null).TrimEnd('.') + '-backups'
        if (Test-Path -LiteralPath $backups) { Remove-Item -LiteralPath $backups -Recurse -Force }
    }
}

# Checks the settings this script depends on before changing anything.
function Test-Project {
    param([string]$ProjectFile)
    $text = [IO.File]::ReadAllText($ProjectFile)
    $checks = [ordered]@{
        'an x64 package'                 = "BuildKey=`"$BuildName`"[^>]*MsiPackageType=`"x64`""
        'a per-user installation'        = "Property=`"ALLUSERS`"[^/]*MultiBuildValue=`"${BuildName}:`""
        'the per-user install folder'    = [regex]::Escape("[LocalAppDataFolder]Programs\$PublisherSlug\$AppSlug")
        'the add custom action'          = 'Action="AddContextMenuCommand" Type="1042"'
        'the remove custom action'       = 'Action="RemoveContextMenuCommand" Type="1106"'
        "the $ExecutableName file entry" = "File=`"$([regex]::Escape($ExecutableFileKey))`""
    }
    foreach ($check in $checks.GetEnumerator()) {
        if ($text -notmatch $check.Value) {
            throw "The AIP at '$ProjectFile' does not have $($check.Key). Fix it in Advanced Installer before building."
        }
    }
}

$root = (Resolve-Path -LiteralPath $ProjectRoot).Path
$script:AdvancedInstallerPath = Resolve-AdvancedInstallerPath $AdvancedInstaller
$payload = Resolve-UnderRoot $root $SourceDirectory
$packaging = Resolve-UnderRoot $root $PackagingDirectory
$output = Resolve-UnderRoot $root $OutputDirectory
$iconFile = Join-Path $root 'assets\plain-path-copy.ico'
$projectFile = Join-Path $packaging "$AppSlug.aip"

$version = Get-AppVersion $root
Test-Payload $payload
if (-not (Test-Path -LiteralPath $iconFile)) {
    throw "The installer icon '$iconFile' is missing. Run scripts/make-icons.ps1."
}
New-Item -ItemType Directory -Force -Path $packaging, $output | Out-Null

if (-not (Test-Path -LiteralPath $projectFile)) {
    Initialize-Project $projectFile $payload $iconFile
}
Test-Project $projectFile

$installer = Join-Path $output "$AppSlug-$version-setup.exe"
if (Test-Path -LiteralPath $installer) { Remove-Item -LiteralPath $installer -Force }
Invoke-InstallerCli @('/edit', $projectFile, '/RefreshSync', 'APPDIR')
Invoke-InstallerCli @('/edit', $projectFile, '/SetVersion', $version)
Invoke-InstallerCli @('/edit', $projectFile, '/SetPackageName', "$AppSlug-$version-setup.exe", '-buildname', $BuildName)
Invoke-InstallerCli @('/edit', $projectFile, '/SetOutputLocation', '-buildname', $BuildName, '-path', $output)
Invoke-InstallerCli @('/rebuild', $projectFile, '-buildslist', $BuildName)

if (-not (Test-Path -LiteralPath $installer) -or (Get-Item -LiteralPath $installer).Length -eq 0) {
    throw "The build reported success but '$installer' was not written."
}
$signature = Get-AuthenticodeSignature -LiteralPath $installer
Write-Output "Project:   $projectFile"
Write-Output "Installer: $installer"
Write-Output "Signature: $($signature.Status)$(if ($signature.SignerCertificate) { ' by ' + $signature.SignerCertificate.Subject.Split(',')[0] })"
