@echo off
REM License Checker - Automated Build Script for Windows 7+
REM This script builds the entire License Checker project

setlocal enabledelayedexpansion

echo.
echo ================================
echo License Checker - Build Script
echo ================================
echo.

REM Check if Qt5 path is set
if not defined Qt5_DIR (
    echo ERROR: Qt5_DIR environment variable is not set
    echo.
    echo Please set Qt5_DIR before running this script:
    echo   set Qt5_DIR=C:\Qt\5.15.0\msvc2019_64
    echo.
    exit /b 1
)

echo Qt5 Directory: %Qt5_DIR%

REM Check if CMake is available
cmake --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: CMake is not installed or not in PATH
    echo Please install CMake from https://cmake.org/download/
    exit /b 1
)

echo CMake found: OK
echo.

REM Create build directory
if not exist "build" (
    echo Creating build directory...
    mkdir build
)

cd build

REM Run CMake configuration
echo Configuring CMake...
cmake -G "Visual Studio 16 2019" -A x64 -DCMAKE_PREFIX_PATH="%Qt5_DIR%" ..

if errorlevel 1 (
    echo ERROR: CMake configuration failed
    cd ..
    exit /b 1
)

echo.
echo Building project (Release)...
cmake --build . --config Release

if errorlevel 1 (
    echo ERROR: Build failed
    cd ..
    exit /b 1
)

cd ..

echo.
echo ================================
echo Build completed successfully!
echo ================================
echo.
echo Output files:
echo   UI Application: build\src\ui\Release\LicenseCheckerUI.exe
echo   Service/Agent:  build\src\agent\Release\LicenseCheckerAgent.exe
echo.
echo To run the UI:
echo   build\src\ui\Release\LicenseCheckerUI.exe
echo.
echo To install service (run as Administrator):
echo   build\src\agent\Release\LicenseCheckerAgent.exe /install
echo.
pause
