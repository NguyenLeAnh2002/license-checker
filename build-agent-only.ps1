# License Checker Agent - PowerShell Build Script

param(
    [string]$Qt5Path = "C:\Qt\5.15.0\mingw81_64",
    [switch]$Test = $false,
    [switch]$Install = $false
)

Write-Host "================================" -ForegroundColor Cyan
Write-Host "Build Agent Service Only" -ForegroundColor Cyan
Write-Host "================================" -ForegroundColor Cyan
Write-Host ""

# Verify Qt5
if (-not (Test-Path $Qt5Path)) {
    Write-Host "ERROR: Qt5 not found at $Qt5Path" -ForegroundColor Red
    Write-Host ""
    Write-Host "Usage:" -ForegroundColor Yellow
    Write-Host "  `$env:Qt5_DIR = 'C:\Qt\5.15.0\mingw81_64'" -ForegroundColor Gray
    Write-Host "  .\build-agent-only.ps1" -ForegroundColor Gray
    exit 1
}

if (-not (Test-Path "$Qt5Path\bin\qmake.exe")) {
    Write-Host "ERROR: qmake not found" -ForegroundColor Red
    exit 1
}

Write-Host "Qt5: $Qt5Path" -ForegroundColor Green
$env:PATH = "$Qt5Path\bin;$env:PATH"

Write-Host ""

# Clean old build
if (Test-Path "build-agent") {
    Write-Host "Cleaning old build..." -ForegroundColor Yellow
    Remove-Item -Path "build-agent" -Recurse -Force -ErrorAction SilentlyContinue
}

# Create build directory
New-Item -ItemType Directory -Path "build-agent" -Force | Out-Null
Set-Location build-agent

Write-Host "Configuring with qmake..." -ForegroundColor Cyan
& qmake -spec win32-g++ CONFIG+=release ..\AgentOnly.pro

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: qmake failed" -ForegroundColor Red
    Set-Location ..
    exit 1
}

Write-Host "Building..." -ForegroundColor Cyan
& mingw32-make -j4

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Build failed" -ForegroundColor Red
    Set-Location ..
    exit 1
}

Set-Location ..

Write-Host ""
Write-Host "================================" -ForegroundColor Green
Write-Host "BUILD SUCCESS!" -ForegroundColor Green
Write-Host "================================" -ForegroundColor Green
Write-Host ""

$exePath = "build-agent\release\LicenseCheckerAgent.exe"
Write-Host "Output: $exePath" -ForegroundColor Cyan
Write-Host ""

# Test if requested
if ($Test) {
    Write-Host "Running service in test mode..." -ForegroundColor Yellow
    & $exePath /test
}

# Install if requested
if ($Install) {
    if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        Write-Host "ERROR: Administrator privileges required for /install" -ForegroundColor Red
        Write-Host "Run PowerShell as Administrator" -ForegroundColor Yellow
        exit 1
    }

    Write-Host "Installing service..." -ForegroundColor Yellow
    & $exePath /install
    Start-Sleep -Seconds 2

    Write-Host "Starting service..." -ForegroundColor Yellow
    & net start LicenseCheckerAgent

    Write-Host "Service installed and started" -ForegroundColor Green
}

Write-Host ""
Write-Host "Usage:" -ForegroundColor Yellow
Write-Host "  Test:    $exePath /test" -ForegroundColor Gray
Write-Host "  Install: $exePath /install" -ForegroundColor Gray
Write-Host "  Start:   net start LicenseCheckerAgent" -ForegroundColor Gray
Write-Host "  Stop:    net stop LicenseCheckerAgent" -ForegroundColor Gray
Write-Host "  Uninstall: $exePath /uninstall" -ForegroundColor Gray
Write-Host ""
