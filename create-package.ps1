# License Checker UI - Create Distribution Package

param(
    [string]$OutputPath = "C:\LicenseChecker-UI-Package"
)

Write-Host "================================" -ForegroundColor Cyan
Write-Host "Creating UI Package" -ForegroundColor Cyan
Write-Host "================================" -ForegroundColor Cyan
Write-Host ""

# Create output directory
if (Test-Path $OutputPath) {
    Write-Host "Removing old package..." -ForegroundColor Yellow
    Remove-Item -Path $OutputPath -Recurse -Force
}

Write-Host "Creating package at: $OutputPath" -ForegroundColor Green
New-Item -ItemType Directory -Path $OutputPath | Out-Null

# Create subdirectories
@("src/ui", "src/common") | ForEach-Object {
    $path = Join-Path $OutputPath $_
    New-Item -ItemType Directory -Path $path -Force | Out-Null
}

Write-Host ""
Write-Host "Copying files..." -ForegroundColor Cyan

# Copy root files
$rootFiles = @(
    "UIOnly.pro",
    "build-ui-only.bat",
    "build-ui-only.ps1",
    "PACKAGE_CONTENTS.md"
)

foreach ($file in $rootFiles) {
    $src = Join-Path (Get-Location) $file
    $dst = Join-Path $OutputPath $file

    if (Test-Path $src) {
        Copy-Item -Path $src -Destination $dst -Force
        Write-Host "  ✓ $file"
    } else {
        Write-Host "  ✗ $file (not found)" -ForegroundColor Yellow
    }
}

Write-Host ""

# Copy UI files
$uiFiles = @(
    "src/ui/main.cpp",
    "src/ui/MainWindow.h",
    "src/ui/MainWindow.cpp",
    "src/ui/SystemTrayIcon.h",
    "src/ui/SystemTrayIcon.cpp",
    "src/ui/LicenseDataModel.h",
    "src/ui/LicenseDataModel.cpp"
)

Write-Host "UI Files:" -ForegroundColor Cyan
foreach ($file in $uiFiles) {
    $src = Join-Path (Get-Location) $file
    $dst = Join-Path $OutputPath $file

    if (Test-Path $src) {
        Copy-Item -Path $src -Destination $dst -Force
        Write-Host "  ✓ $(Split-Path $file -Leaf)"
    } else {
        Write-Host "  ✗ $(Split-Path $file -Leaf) (not found)" -ForegroundColor Yellow
    }
}

Write-Host ""

# Copy Common files
$commonFiles = @(
    "src/common/NamedPipeClient.h",
    "src/common/NamedPipeClient.cpp"
)

Write-Host "Common Files:" -ForegroundColor Cyan
foreach ($file in $commonFiles) {
    $src = Join-Path (Get-Location) $file
    $dst = Join-Path $OutputPath $file

    if (Test-Path $src) {
        Copy-Item -Path $src -Destination $dst -Force
        Write-Host "  ✓ $(Split-Path $file -Leaf)"
    } else {
        Write-Host "  ✗ $(Split-Path $file -Leaf) (not found)" -ForegroundColor Yellow
    }
}

Write-Host ""
Write-Host "================================" -ForegroundColor Green
Write-Host "Package created successfully!" -ForegroundColor Green
Write-Host "================================" -ForegroundColor Green
Write-Host ""
Write-Host "Location: $OutputPath" -ForegroundColor Cyan
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "1. Copy this folder to another machine"
Write-Host "2. Ensure Qt5.15 MinGW is installed"
Write-Host "3. Run: build-ui-only.bat"
Write-Host ""

# Show summary
$fileCount = (Get-ChildItem -Path $OutputPath -Recurse -File).Count
$folderSize = (Get-ChildItem -Path $OutputPath -Recurse | Measure-Object -Sum Length).Sum / 1MB

Write-Host "Summary:" -ForegroundColor Cyan
Write-Host "  Files: $fileCount"
Write-Host "  Size: $([math]::Round($folderSize, 2)) MB"
Write-Host ""

Write-Host "To compress into ZIP:" -ForegroundColor Yellow
Write-Host "  Compress-Archive -Path '$OutputPath' -DestinationPath '$OutputPath.zip'" -ForegroundColor Gray
