# Build License Checker with Visual Studio (no Qt required)

> This is the current, working build path. `LicenseChecker.pro` (qmake),
> `CMakeLists.txt`, `build.bat`, `build-mingw.bat`/`.ps1` and the other
> `BUILD_*.md`/`QUICK_BUILD.md` docs describe an older Qt-based UI +
> separate Windows-service agent architecture (`src/ui`, `src/agent`,
> `src/common`) that has since been replaced by a single native app in
> `src/ui-native` and no longer matches this repository - don't follow them.

## Quick Start

### Option 1: Using the build script (easiest)
```batch
build-msbuild.bat
```

### Option 2: Using the Visual Studio GUI
```
1. Open LicenseChecker.sln in Visual Studio
2. Set configuration: Release | x64
3. Build → Build Solution (Ctrl+Shift+B)
4. Output: x64\Release\LicenseCheckerUI.exe
```

### Option 3: Command line
```batch
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" ^
  LicenseChecker.sln /p:Configuration=Release /p:Platform=x64
```

---

## Prerequisites

- **Visual Studio 2022** (or standalone "Build Tools for Visual Studio 2022")
  with the **"Desktop development with C++"** workload, which includes the
  Windows SDK.
- The individual component **"MSVC v142 - VS 2019 C++ x64/x86 build
  tools"** - both `.vcxproj` files in this repo target toolset `v142`.
  (Retargeting to v143 also works if you'd rather not install v142, since
  the code has no v142-specific dependency - but v142 is what both repo
  machines currently have, so that's the supported path.)

**No Qt, no CMake, no qmake needed.** Everything this solution links against
(`advapi32.lib`, `kernel32.lib`, `user32.lib`, `gdi32.lib`, `comctl32.lib`,
`gdiplus.lib`, `dwmapi.lib`, `ws2_32.lib`, `wbemuuid.lib`, `ole32.lib`,
`oleaut32.lib`, `crypt32.lib`, `wtsapi32.lib`, `winhttp.lib`) ships with the
Windows SDK that the C++ workload installs.

---

## Project Structure

```
LicenseChecker.sln
├── src\license-detection\LicenseDetection.vcxproj   (static library)
│     LicenseResult, LicenseDetector, SLAPIDetector, WMIDetector,
│     RegistryDetector, DetectionLogger, OfficeOSPPDetector, ...
└── src\ui-native\LicenseCheckerUI.vcxproj            (application, depends on the above)
      main.cpp (WinMain), MainWindow, DetectionWorker, ServerReporter,
      StartupLog, Localization
```

`LicenseCheckerUI.exe` is a single standalone GUI application - there is no
separate agent/service executable, no named-pipe IPC, and nothing to
`/install` as a Windows service in the current source tree. Just run the
exe directly; it runs its own detection cycle and reports to the
controller via `ServerReporter` internally (see `AGENT_SERVER_PROTOCOL_REAL.md`).

- **Platform:** x64 only
- **Configurations:** Debug, Release
- **Toolset:** v142
- **C++ Standard:** C++17
- **Character set:** Unicode

---

## Build Output

```
x64\
├── Release\
│   ├── LicenseCheckerUI.exe     ← run this
│   ├── LicenseDetection.lib
│   └── *.obj
└── Debug\
    └── ...
```

## Test the Built Executable

```batch
x64\Release\LicenseCheckerUI.exe
```

---

## Troubleshooting

### Error: "Cannot find msbuild.exe"
Visual Studio / Build Tools not installed, or not in PATH.
```batch
set PATH=%PATH%;C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin
build-msbuild.bat
```

### Error: "The build tools for v142 (Platform Toolset = 'v142') cannot be found"
Install the individual component **"MSVC v142 - VS 2019 C++ x64/x86 build
tools"** via the Visual Studio Installer (Individual components tab).

### Error: "Cannot find project file"
Run from the repository root (where `LicenseChecker.sln` lives).

### Error: "Unresolved external symbols"
`LicenseDetection` must build first (it's a dependency of `LicenseCheckerUI`
via the solution's project dependency, not build order alone) - do a full
solution build/rebuild rather than building `LicenseCheckerUI.vcxproj` in
isolation.

---

## Command Line Builds

```batch
MSBuild LicenseChecker.sln /p:Configuration=Release /p:Platform=x64 /t:Clean
MSBuild LicenseChecker.sln /p:Configuration=Release /p:Platform=x64 /t:Build
MSBuild LicenseChecker.sln /p:Configuration=Release /p:Platform=x64 /t:Rebuild
MSBuild LicenseChecker.sln /p:Configuration=Release /p:Platform=x64 /v:detailed
```
