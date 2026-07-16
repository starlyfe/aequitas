#Requires -Version 5.1
<#
.SYNOPSIS
    Configure, build, and test Aequitas on Windows.

.PARAMETER Debug
    Build Debug instead of Release.

.PARAMETER Run
    Launch aequitas.exe after a successful build.

.EXAMPLE
    .\scripts\build-windows.ps1
    .\scripts\build-windows.ps1 --debug
    .\scripts\build-windows.ps1 --run
#>
[CmdletBinding()]
param(
    [switch]$Debug,
    [switch]$Run
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $RepoRoot

function Write-Step([string]$Message) {
    Write-Host "`n==> $Message" -ForegroundColor Cyan
}

function Find-CMake {
    $cmake = Get-Command cmake -ErrorAction SilentlyContinue
    if (-not $cmake) {
        Write-Error @"
CMake not found on PATH.
Install CMake 3.28+ from https://cmake.org/download/ and ensure it is on PATH.
"@
    }
    $version = & cmake --version | Select-Object -First 1
    Write-Host "Found $version"
    if ($version -notmatch 'version (\d+)\.(\d+)') {
        Write-Error "Could not parse CMake version."
    }
    $major = [int]$Matches[1]
    $minor = [int]$Matches[2]
    if ($major -lt 3 -or ($major -eq 3 -and $minor -lt 28)) {
        Write-Error "CMake 3.28+ required (found $major.$minor)."
    }
}

function Resolve-VulkanSdk {
    if ($env:VULKAN_SDK -and (Test-Path $env:VULKAN_SDK)) {
        Write-Host "VULKAN_SDK = $env:VULKAN_SDK"
        return $env:VULKAN_SDK
    }

    $candidates = Get-ChildItem -Path 'C:\VulkanSDK' -Directory -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending

    foreach ($dir in $candidates) {
        $loader = Join-Path $dir.FullName 'Bin\glslc.exe'
        if (Test-Path $loader) {
            $env:VULKAN_SDK = $dir.FullName
            Write-Host "Auto-detected VULKAN_SDK = $env:VULKAN_SDK"
            return $env:VULKAN_SDK
        }
    }

    Write-Error @"
VULKAN_SDK is not set and no SDK was found under C:\VulkanSDK\.
Install the LunarG Vulkan SDK from https://vulkan.lunarg.com/ and set VULKAN_SDK
to the SDK root (e.g. C:\VulkanSDK\1.4.xxx.x).
"@
}

function Test-Ninja {
    return [bool](Get-Command ninja -ErrorAction SilentlyContinue)
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
Write-Step 'Checking prerequisites'
Find-CMake
Resolve-VulkanSdk | Out-Null

$BuildType = if ($Debug) { 'debug' } else { 'release' }
$Preset    = $BuildType
$BuildPreset = $BuildType
$TestPreset  = $BuildType

if (-not (Test-Ninja)) {
    Write-Host 'Ninja not found — falling back to Visual Studio preset.' -ForegroundColor Yellow
    $Preset = 'windows-vs'
    if ($Debug) {
        $BuildPreset = 'windows-vs-debug'
    } else {
        $BuildPreset = 'windows-vs-release'
    }
    $TestPreset = if ($Debug) { 'debug' } else { 'release' }
}

$BuildDir = Join-Path $RepoRoot "build\$BuildType"
$BinDir   = Join-Path $BuildDir 'bin'

Write-Step "Configuring preset '$Preset'"
& cmake --preset $Preset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Step "Building preset '$BuildPreset'"
& cmake --build --preset $BuildPreset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Step "Running tests (preset '$TestPreset')"
& ctest --preset $TestPreset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$Headless = Join-Path $BinDir 'aequitas_headless.exe'
$Visual   = Join-Path $BinDir 'aequitas.exe'

Write-Step 'Build complete'
Write-Host "  aequitas_headless : $Headless"
if (Test-Path $Visual) {
    Write-Host "  aequitas          : $Visual"
} else {
    Write-Host "  aequitas          : (not built — AEQUITAS_BUILD_RENDERER=OFF?)" -ForegroundColor Yellow
}

if ($Run) {
    if (-not (Test-Path $Visual)) {
        Write-Error "Cannot --run: $Visual not found."
    }
    Write-Step 'Launching aequitas'
    & $Visual
}
