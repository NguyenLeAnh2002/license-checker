@echo off
REM License Checker - Build Script for MinGW/Qt5
REM This script builds the entire project using qmake and mingw32-make

setlocal enabledelayedexpansion

echo.
echo ================================
echo License Checker - MinGW Build
echo ================================
echo.

REM Check Qt5 installation
if not defined Qt5_DIR (
    echo ERROR: Qt5_DIR environment variable is not set
    echo.
    echo Set Qt5_DIR to your Qt5 MinGW installation:
    echo   set Qt5_DIR=C:\Qt\5.15.0\mingw81_64
    echo.
    echo Example:
    echo   set Qt5_DIR=C:\Qt\5.15.0\mingw81_64
    echo   build-mingw.bat
    echo.
    exit /b 1
)

echo Qt5 Directory: %Qt5_DIR%

REM Check if qmake exists
if not exist "%Qt5_DIR%\bin\qmake.exe" (
    echo ERROR: qmake not found in %Qt5_DIR%\bin\
    echo Please verify Qt5 installation path
    exit /b 1
)

if not exist "%Qt5_DIR%\bin\mingw32-make.exe" (
    echo ERROR: mingw32-make not found in %Qt5_DIR%\bin\
    echo Please install Qt5 with MinGW support
    exit /b 1
)

echo qmake found: OK
echo mingw32-make found: OK
echo.

REM Add Qt to PATH
set PATH=%Qt5_DIR%\bin;%PATH%

REM Clean previous build (optional)
if exist "build" (
    echo Cleaning previous build...
    cd build
    mingw32-make distclean >nul 2>&1
    cd ..
)

REM Create build directory
if not exist "build" (
    echo Creating build directory...
    mkdir build
)

cd build

REM Run qmake
echo Configuring project with qmake...
qmake -r -spec win32-g++ CONFIG+=release ..\LicenseChecker.pro

if errorlevel 1 (
    echo ERROR: qmake configuration failed
    echo Check Qt5 path and try again
    cd ..
    exit /b 1
)

echo.
echo Building with MinGW (parallel jobs)...
mingw32-make -j4

if errorlevel 1 (
    echo ERROR: Build failed
    echo Check error messages above
    cd ..
    pause
    exit /b 1
)

cd ..

echo.
echo ================================
echo Build completed successfully!
echo ================================
echo.
echo Output files:
echo   UI Application: build\src\ui\LicenseCheckerUI.exe
echo   Service/Agent:  build\src\agent\LicenseCheckerAgent.exe
echo.
echo To run the UI:
echo   build\src\ui\LicenseCheckerUI.exe
echo.
echo To install service (run as Administrator):
echo   build\src\agent\LicenseCheckerAgent.exe /install
echo   net start LicenseCheckerAgent
echo.
pause
