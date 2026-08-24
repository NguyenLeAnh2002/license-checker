@echo off
REM License Checker Agent - Build with MSBuild
REM Builds for Visual Studio 2022

echo.
echo ================================
echo Build Agent Service - MSBuild
echo ================================
echo.

REM Find Visual Studio 2022
set MSBUILD_PATH=
for /f "tokens=*" %%A in ('where msbuild.exe') do set MSBUILD_PATH=%%A

if "%MSBUILD_PATH%"=="" (
    echo ERROR: MSBuild not found in PATH
    echo.
    echo Please ensure Visual Studio 2022 is installed with C++ workload
    echo.
    echo Or manually set MSBUILD_PATH:
    echo   set MSBUILD_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe
    echo.
    exit /b 1
)

echo MSBuild: %MSBUILD_PATH%
echo.

REM Build
echo Building LicenseCheckerAgent.sln...
"%MSBUILD_PATH%" LicenseCheckerAgent.sln /p:Configuration=Release /p:Platform=x64 /m

if errorlevel 1 (
    echo.
    echo ERROR: Build failed
    echo.
    pause
    exit /b 1
)

echo.
echo ================================
echo BUILD SUCCESS!
echo ================================
echo.
echo Output: x64\Release\LicenseCheckerAgent.exe
echo.
echo Next steps:
echo   1. Test the service:
echo      x64\Release\LicenseCheckerAgent.exe /test
echo.
echo   2. Install as service (Admin):
echo      x64\Release\LicenseCheckerAgent.exe /install
echo      net start LicenseCheckerAgent
echo.
echo   3. Check status:
echo      sc query LicenseCheckerAgent
echo.
echo   4. Stop service:
echo      net stop LicenseCheckerAgent
echo.
echo   5. Uninstall:
echo      x64\Release\LicenseCheckerAgent.exe /uninstall
echo.
pause
