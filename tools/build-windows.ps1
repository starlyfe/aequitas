#Requires -Version 5.1
<#
.SYNOPSIS
    Configure, build, test, and optionally package Aequitas on Windows.

.EXAMPLE
    .\tools\build-windows.ps1
    .\tools\build-windows.ps1 --dev --run
    .\tools\build-windows.ps1 --debug --package
    .\tools\build-windows.ps1 --no-package --no-test
#>
param(
    [switch]$BuildDebug,
    [switch]$BuildDev,
    [switch]$Run,
    [switch]$Package,
    [switch]$NoPackage,
    [switch]$NoTest,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Remaining = @()
)

foreach ($arg in $Remaining) {
    switch ($arg) {
        '--debug'      { $BuildDebug = $true }
        '-Debug'       { $BuildDebug = $true }
        '--dev'        { $BuildDev = $true }
        '--run'        { $Run = $true }
        '--package'    { $Package = $true }
        '--no-package' { $NoPackage = $true }
        '--no-test'    { $NoTest = $true }
        default        { Write-Error "Unknown argument: $arg" }
    }
}

Set-StrictMode -Version Latest
# cmake writes status to stderr; only treat nonzero exit as failure.
$ErrorActionPreference = 'Continue'

$RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $RepoRoot

function Write-Step([string]$Message) {
    Write-Host "`n==> $Message" -ForegroundColor Cyan
}

function Read-ProjectVersion {
    $v = (Get-Content -Path (Join-Path $RepoRoot 'VERSION') -Raw).Trim()
    if ($v -notmatch '^\d+\.\d+\.\d+$') {
        Write-Error "VERSION file must contain SemVer MAJOR.MINOR.PATCH (got '$v')"
    }
    return $v
}

function Find-CMake {
    $cmake = Get-Command cmake -ErrorAction SilentlyContinue
    if (-not $cmake) {
        Write-Error "CMake not found on PATH. Install CMake 3.28+ from https://cmake.org/download/"
    }
    $version = & cmake --version | Select-Object -First 1
    Write-Host "Found $version"
}

function Resolve-VulkanSdk {
    if ($env:VULKAN_SDK -and (Test-Path $env:VULKAN_SDK)) {
        Write-Host "VULKAN_SDK = $env:VULKAN_SDK"
        return
    }
    $candidates = Get-ChildItem -Path 'C:\VulkanSDK' -Directory -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending
    foreach ($dir in $candidates) {
        if (Test-Path (Join-Path $dir.FullName 'Bin\glslc.exe')) {
            $env:VULKAN_SDK = $dir.FullName
            Write-Host "Auto-detected VULKAN_SDK = $env:VULKAN_SDK"
            return
        }
    }
    Write-Error "VULKAN_SDK is not set and no SDK was found under C:\VulkanSDK\."
}

function Test-Ninja {
    return [bool](Get-Command ninja -ErrorAction SilentlyContinue)
}

Write-Step 'Checking prerequisites'
Find-CMake
Resolve-VulkanSdk
$ProjectVersion = Read-ProjectVersion
Write-Host "Project VERSION = $ProjectVersion"

# Profile selection: --dev > --debug > release
if ($BuildDev) {
    $Preset = 'dev'
    $BuildPreset = 'dev'
    $TestPreset = 'dev'
    $IsReleaseLike = $false
} elseif ($BuildDebug) {
    $Preset = 'debug'
    $BuildPreset = 'debug'
    $TestPreset = 'debug'
    $IsReleaseLike = $false
} else {
    $Preset = 'release'
    $BuildPreset = 'release'
    $TestPreset = 'release'
    $IsReleaseLike = $true
}

if (-not (Test-Ninja)) {
    Write-Host 'Ninja not found - falling back to Visual Studio preset.' -ForegroundColor Yellow
    $Preset = 'windows-vs'
    if ($BuildDev -or $BuildDebug) {
        $BuildPreset = 'windows-vs-debug'
        $TestPreset = 'debug'
    } else {
        $BuildPreset = 'windows-vs-release'
        $TestPreset = 'release'
    }
}

$BuildDir = Join-Path $RepoRoot "build\$($BuildPreset -replace '^windows-vs-','')"
if ($BuildPreset -eq 'windows-vs-debug') { $BuildDir = Join-Path $RepoRoot 'build\windows-vs' }
elseif ($BuildPreset -eq 'windows-vs-release') { $BuildDir = Join-Path $RepoRoot 'build\windows-vs' }
elseif ($BuildDev) { $BuildDir = Join-Path $RepoRoot 'build\dev' }
elseif ($BuildDebug) { $BuildDir = Join-Path $RepoRoot 'build\debug' }
else { $BuildDir = Join-Path $RepoRoot 'build\release' }
$BinDir = Join-Path $BuildDir 'bin'

Write-Step "Configuring preset '$Preset'"
& cmake --preset $Preset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Step "Building preset '$BuildPreset'"
& cmake --build --preset $BuildPreset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if (-not $NoTest) {
    Write-Step "Running tests (preset '$TestPreset')"
    & ctest --preset $TestPreset
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} else {
    Write-Host "`n(Skipping tests)" -ForegroundColor DarkGray
}

$Headless = Join-Path $BinDir 'aequitas_headless.exe'
$Visual   = Join-Path $BinDir 'aequitas.exe'

Write-Step 'Build complete'
Write-Host "  version           : $ProjectVersion"
Write-Host "  profile           : $Preset"
Write-Host "  aequitas_headless : $Headless"
if (Test-Path $Visual) {
    Write-Host "  aequitas          : $Visual"
} else {
    Write-Host "  aequitas          : (not built - AEQUITAS_BUILD_RENDERER=OFF?)" -ForegroundColor Yellow
}

$ShouldPackage = (-not $NoPackage) -and ($IsReleaseLike -or $Package)
if ($ShouldPackage) {
    Write-Step "Packaging releases/v$ProjectVersion/windows"
    & (Join-Path $RepoRoot 'tools\package-release.ps1') -BinDir $BinDir -Version $ProjectVersion
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} else {
    Write-Host "`n(Skipping releases/ package - use Release, or pass --package)" -ForegroundColor DarkGray
}

if ($Run) {
    if (-not (Test-Path $Visual)) {
        Write-Error "Cannot --run: $Visual not found."
    }
    Write-Step 'Launching aequitas'
    & $Visual
}
