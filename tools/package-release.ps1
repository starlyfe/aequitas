#Requires -Version 5.1
<#
.SYNOPSIS
    Package Release binaries into releases/v$VERSION/windows/ and a zip.

.PARAMETER BinDir
    Path to the CMake bin output directory (contains aequitas.exe, shaders/, ...).

.PARAMETER Version
    Optional override; default reads the repo-root VERSION file.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$BinDir,

    [string]$Version = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)

if (-not $Version) {
    $Version = (Get-Content -Path (Join-Path $RepoRoot 'VERSION') -Raw).Trim()
}

if ($Version -notmatch '^\d+\.\d+\.\d+$') {
    Write-Error "Invalid VERSION '$Version' (expected MAJOR.MINOR.PATCH)"
}

$OutRoot = Join-Path $RepoRoot "releases\v$Version"
$OutDir  = Join-Path $OutRoot 'windows'
$ZipPath = Join-Path $OutRoot "aequitas-v$Version-windows.zip"

Write-Host "`n==> Packaging Aequitas v$Version -> $OutDir" -ForegroundColor Cyan

if (Test-Path $OutDir) {
    Remove-Item -Recurse -Force $OutDir
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

foreach ($name in @('aequitas.exe', 'aequitas_headless.exe')) {
    $src = Join-Path $BinDir $name
    if (Test-Path $src) {
        Copy-Item -Force $src $OutDir
    }
}

$shaders = Join-Path $BinDir 'shaders'
if (Test-Path $shaders) {
    Copy-Item -Recurse -Force $shaders (Join-Path $OutDir 'shaders')
}

Set-Content -Path (Join-Path $OutRoot 'VERSION.txt') -Value $Version -NoNewline
Set-Content -Path (Join-Path $OutDir 'PLATFORM.txt') -Value 'windows' -NoNewline
Set-Content -Path (Join-Path $OutDir 'BUILT_AT.txt') -Value ([DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ')) -NoNewline

# NOTES.md from CHANGELOG section
$changelog = Get-Content -Path (Join-Path $RepoRoot 'CHANGELOG.md') -Raw
$pattern = "(?ms)^## \[$([regex]::Escape($Version))\].*?(?=^## |\z)"
$match = [regex]::Match($changelog, $pattern)
$notes = if ($match.Success) { $match.Value.Trim() + "`n" } else { "## [$Version]`n`n(No CHANGELOG section found.)`n" }
Set-Content -Path (Join-Path $OutRoot 'NOTES.md') -Value $notes -NoNewline

if (Test-Path $ZipPath) {
    Remove-Item -Force $ZipPath
}

# Compress platform folder + metadata
$stage = Join-Path $OutRoot '_zip_stage'
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Copy-Item -Recurse $OutDir (Join-Path $stage 'windows')
Copy-Item (Join-Path $OutRoot 'VERSION.txt') $stage
Copy-Item (Join-Path $OutRoot 'NOTES.md') $stage
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $ZipPath -Force
Remove-Item -Recurse -Force $stage

Write-Host "  folder : $OutDir"
Write-Host "  zip    : $ZipPath"
