# License Checker - PowerShell Build Script for Windows 7+

param(
    [string]$Qt5Path = "C:\Qt\5.15.0\msvc2019_64",
    [string]$BuildType = "Release",
    [switch]$Install
)

Write-Host "================================" -ForegroundColor Cyan
Write-Host "License Checker - Build Script" -ForegroundColor Cyan
Write-Host "================================" -ForegroundColor Cyan
Write-Host ""

# Check Qt5 installation
if (-not (Test-Path $Qt5Path)) {
    Write-Host "ERROR: Qt5 not found at $Qt5Path" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please specify correct Qt5 path:" -ForegroundColor Yellow
    Write-Host "  .\build.ps1 -Qt5Path 'C:\Qt\5.15.0\msvc2019_64'" -ForegroundColor Gray
    exit 1
}

Write-Host "Qt5 Directory: $Qt5Path" -ForegroundColor Green

# Check CMake
$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmake) {
    Write-Host "ERROR: CMake is not installed or not in PATH" -ForegroundColor Red
    Write-Host "Install from https://cmake.org/download/" -ForegroundColor Yellow
    exit 1
}

Write-Host "CMake found: $($cmake.Source)" -ForegroundColor Green
Write-Host ""

# Create build directory
if (-not (Test-Path "build")) {
    Write-Host "Creating build directory..." -ForegroundColor Cyan
    New-Item -ItemType Directory -Path "build" | Out-Null
}

Set-Location build

# Configure CMake
Write-Host "Configuring CMake..." -ForegroundColor Cyan
$cmakeCmd = @(
    "-G", "Visual Studio 16 2019",
    "-A", "x64",
    "-DCMAKE_PREFIX_PATH=`"$Qt5Path`"",
    ".."
)

& cmake @cmakeCmd
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: CMake configuration failed" -ForegroundColor Red
    Set-Location ..
    exit 1
}

Write-Host ""
Write-Host "Building project ($BuildType)..." -ForegroundColor Cyan
& cmake --build . --config $BuildType

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
Write-Host "  UI Application: build\src\ui\Release\LicenseCheckerUI.exe"
Write-Host "  Service/Agent:  build\src\agent\Release\LicenseCheckerAgent.exe"
Write-Host ""

Write-Host "To run the UI:" -ForegroundColor Yellow
Write-Host "  .\build\src\ui\Release\LicenseCheckerUI.exe"
Write-Host ""

Write-Host "To install service (run PowerShell as Administrator):" -ForegroundColor Yellow
Write-Host "  .\build\src\agent\Release\LicenseCheckerAgent.exe /install"
Write-Host "  net start LicenseCheckerAgent"
Write-Host ""

if ($Install) {
    Write-Host "Installing service..." -ForegroundColor Cyan
    $agentExe = ".\build\src\agent\Release\LicenseCheckerAgent.exe"

    if (Test-Path $agentExe) {
        & $agentExe /install
        Start-Sleep -Seconds 2
        net start LicenseCheckerAgent
        Write-Host "Service installed and started" -ForegroundColor Green
    } else {
        Write-Host "ERROR: Agent executable not found" -ForegroundColor Red
    }
}
