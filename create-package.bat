@echo off
REM License Checker UI - Create Distribution Package

setlocal enabledelayedexpansion

set OUTPUT_PATH=C:\LicenseChecker-UI-Package

echo.
echo ================================
echo Creating UI Package
echo ================================
echo.

REM Check if output path exists
if exist "%OUTPUT_PATH%" (
    echo Removing old package...
    rmdir /s /q "%OUTPUT_PATH%"
)

echo Creating package at: %OUTPUT_PATH%
mkdir "%OUTPUT_PATH%"
mkdir "%OUTPUT_PATH%\src\ui"
mkdir "%OUTPUT_PATH%\src\common"

echo.
echo Copying files...
echo.

REM Copy root files
echo Root Files:
copy "UIOnly.pro" "%OUTPUT_PATH%\" >nul && echo   * UIOnly.pro
copy "build-ui-only.bat" "%OUTPUT_PATH%\" >nul && echo   * build-ui-only.bat
copy "build-ui-only.ps1" "%OUTPUT_PATH%\" >nul && echo   * build-ui-only.ps1
copy "PACKAGE_CONTENTS.md" "%OUTPUT_PATH%\" >nul && echo   * PACKAGE_CONTENTS.md

echo.
echo UI Files:
copy "src\ui\main.cpp" "%OUTPUT_PATH%\src\ui\" >nul && echo   * main.cpp
copy "src\ui\MainWindow.h" "%OUTPUT_PATH%\src\ui\" >nul && echo   * MainWindow.h
copy "src\ui\MainWindow.cpp" "%OUTPUT_PATH%\src\ui\" >nul && echo   * MainWindow.cpp
copy "src\ui\SystemTrayIcon.h" "%OUTPUT_PATH%\src\ui\" >nul && echo   * SystemTrayIcon.h
copy "src\ui\SystemTrayIcon.cpp" "%OUTPUT_PATH%\src\ui\" >nul && echo   * SystemTrayIcon.cpp
copy "src\ui\LicenseDataModel.h" "%OUTPUT_PATH%\src\ui\" >nul && echo   * LicenseDataModel.h
copy "src\ui\LicenseDataModel.cpp" "%OUTPUT_PATH%\src\ui\" >nul && echo   * LicenseDataModel.cpp

echo.
echo Common Files:
copy "src\common\NamedPipeClient.h" "%OUTPUT_PATH%\src\common\" >nul && echo   * NamedPipeClient.h
copy "src\common\NamedPipeClient.cpp" "%OUTPUT_PATH%\src\common\" >nul && echo   * NamedPipeClient.cpp

echo.
echo ================================
echo Package created successfully!
echo ================================
echo.
echo Location: %OUTPUT_PATH%
echo.
echo Next steps:
echo 1. Copy this folder to another machine
echo 2. Ensure Qt5.15 MinGW is installed
echo 3. Run: build-ui-only.bat
echo.
pause
