@echo off
REM License Checker Agent - Build Script
REM Builds only the Windows Service (no UI)

echo.
echo ================================
echo Build Agent Service Only
echo ================================
echo.

REM Check Qt5
if not defined Qt5_DIR (
    echo ERROR: Qt5_DIR environment variable not set
    echo.
    echo Set it first:
    echo   set Qt5_DIR=C:\Qt\5.15.0\mingw81_64
    echo.
    exit /b 1
)

echo Qt5: %Qt5_DIR%

REM Check if qmake exists
if not exist "%Qt5_DIR%\bin\qmake.exe" (
    echo ERROR: qmake not found at %Qt5_DIR%\bin\qmake.exe
    exit /b 1
)

REM Check if mingw32-make exists
if not exist "%Qt5_DIR%\bin\mingw32-make.exe" (
    echo ERROR: mingw32-make not found
    exit /b 1
)

REM Add Qt to PATH
set PATH=%Qt5_DIR%\bin;%PATH%

REM Clean old build
if exist "build-agent" (
    echo Cleaning old build...
    cd build-agent
    mingw32-make distclean >nul 2>&1
    cd ..
    rmdir /s /q build-agent >nul 2>&1
)

mkdir build-agent >nul 2>&1
cd build-agent

echo.
echo Configuring with qmake...
qmake -spec win32-g++ CONFIG+=release ..\AgentOnly.pro

if errorlevel 1 (
    echo ERROR: qmake configuration failed
    cd ..
    pause
    exit /b 1
)

echo Building...
mingw32-make -j4

if errorlevel 1 (
    echo ERROR: Build failed
    cd ..
    pause
    exit /b 1
)

cd ..

echo.
echo ================================
echo BUILD SUCCESS!
echo ================================
echo.
echo Output: build-agent\release\LicenseCheckerAgent.exe
echo.
echo Next steps:
echo   1. Test the service:
echo      build-agent\release\LicenseCheckerAgent.exe /test
echo.
echo   2. Install as service (Admin):
echo      build-agent\release\LicenseCheckerAgent.exe /install
echo      net start LicenseCheckerAgent
echo.
echo   3. Check status:
echo      sc query LicenseCheckerAgent
echo.
echo   4. Stop service:
echo      net stop LicenseCheckerAgent
echo.
echo   5. Uninstall:
echo      build-agent\release\LicenseCheckerAgent.exe /uninstall
echo.
pause
