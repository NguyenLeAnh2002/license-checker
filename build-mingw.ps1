# ============================================================
# OUTDATED: this builds the old Qt-based UI (src/ui, src/agent,
# src/common via LicenseChecker.pro), which no longer exist in
# this repository - this script will fail immediately.
# Current build: run build-msbuild.bat instead (no Qt needed).
# See BUILD_VS2022.md.
# ============================================================
# License Checker - PowerShell Build Script for MinGW/Qt5

param(
    [string]$Qt5Path = "C:\Qt\5.15.0\mingw81_64",
    [switch]$Release = $true,
    [switch]$Debug = $false,
    [int]$Jobs = 4
)

Write-Host "================================" -ForegroundColor Cyan
Write-Host "License Checker - MinGW Build" -ForegroundColor Cyan
Write-Host "================================" -ForegroundColor Cyan
Write-Host ""

# Check Qt5 installation
if (-not (Test-Path $Qt5Path)) {
    Write-Host "ERROR: Qt5 not found at $Qt5Path" -ForegroundColor Red
    Write-Host ""
    Write-Host "Specify correct Qt5 MinGW path:" -ForegroundColor Yellow
    Write-Host "  .\build-mingw.ps1 -Qt5Path 'C:\Qt\5.15.0\mingw81_64'" -ForegroundColor Gray
    exit 1
}

if (-not (Test-Path "$Qt5Path\bin\qmake.exe")) {
    Write-Host "ERROR: qmake not found in $Qt5Path\bin\" -ForegroundColor Red
    exit 1
}

Write-Host "Qt5 Directory: $Qt5Path" -ForegroundColor Green
Write-Host "qmake found: $Qt5Path\bin\qmake.exe" -ForegroundColor Green

# Add Qt to PATH
$env:PATH = "$Qt5Path\bin;$env:PATH"

Write-Host ""

# Create build directory
if (-not (Test-Path "build")) {
    Write-Host "Creating build directory..." -ForegroundColor Cyan
    New-Item -ItemType Directory -Path "build" | Out-Null
}

Set-Location build

# Determine build config
$BuildConfig = "debug_and_release"
if ($Release -and -not $Debug) {
    $BuildConfig = "release"
}
elseif ($Debug -and -not $Release) {
    $BuildConfig = "debug"
}

Write-Host "Build configuration: $BuildConfig" -ForegroundColor Green
Write-Host ""

# Run qmake
Write-Host "Configuring project with qmake..." -ForegroundColor Cyan
& qmake -r -spec win32-g++ CONFIG+=$BuildConfig ..\LicenseChecker.pro

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: qmake configuration failed" -ForegroundColor Red
    Set-Location ..
    exit 1
}

Write-Host ""
Write-Host "Building with MinGW (using $Jobs parallel jobs)..." -ForegroundColor Cyan
& mingw32-make -j$Jobs

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Build failed" -ForegroundColor Red
    Set-Location ..
    exit 1
}

Set-Location ..

Write-Host ""
Write-Host "================================" -ForegroundColor Green
Write-Host "Build completed successfully!" -ForegroundColor Green
Write-Host "================================" -ForegroundColor Green
Write-Host ""

Write-Host "Output files:" -ForegroundColor Cyan
Write-Host "  UI Application: build\src\ui\LicenseCheckerUI.exe"
Write-Host "  Service/Agent:  build\src\agent\LicenseCheckerAgent.exe"
Write-Host ""

Write-Host "To run the UI:" -ForegroundColor Yellow
Write-Host "  .\build\src\ui\LicenseCheckerUI.exe"
Write-Host ""

Write-Host "To install service (run PowerShell as Administrator):" -ForegroundColor Yellow
Write-Host "  .\build\src\agent\LicenseCheckerAgent.exe /install"
Write-Host "  net start LicenseCheckerAgent"
Write-Host ""
