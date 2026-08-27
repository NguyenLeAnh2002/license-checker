@echo off
REM License Checker UI - Build with MSBuild (Visual Studio 2022, toolset v142)
REM Builds LicenseChecker.sln directly - no Qt required. This is the current,
REM working build path; see BUILD_VS2022.md for details.

echo.
echo ================================
echo Build License Checker UI - MSBuild
echo ================================
echo.

REM Find Visual Studio's MSBuild
set MSBUILD_PATH=
for /f "tokens=*" %%A in ('where msbuild.exe') do set MSBUILD_PATH=%%A

if "%MSBUILD_PATH%"=="" (
    echo ERROR: MSBuild not found in PATH
    echo.
    echo Install "Build Tools for Visual Studio 2022" with the "Desktop
    echo development with C++" workload, plus the individual component
    echo "MSVC v142 - VS 2019 C++ x64/x86 build tools" (this project's
    echo .vcxproj files target v142).
    echo.
    echo Or manually set MSBUILD_PATH:
    echo   set MSBUILD_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe
    echo.
    exit /b 1
)

echo MSBuild: %MSBUILD_PATH%
echo.

REM Build
echo Building LicenseChecker.sln...
"%MSBUILD_PATH%" LicenseChecker.sln /p:Configuration=Release /p:Platform=x64 /m

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
echo Output: x64\Release\LicenseCheckerUI.exe
echo.
echo Run it directly - it's a standalone GUI app with no install/service step:
echo   x64\Release\LicenseCheckerUI.exe
echo.
pause
