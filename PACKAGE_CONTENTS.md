# License Checker - Package Contents

> ⚠️ This replaces the old version of this doc, which listed `UIOnly.pro`,
> `SystemTrayIcon.*`, `LicenseDataModel.*`, `NamedPipeClient.*` and a Qt5
> dependency - none of that exists in this repository anymore. There is no
> separate "UI-only" package: the whole app is one small solution.

## What you need to build the whole app (18 source files + 2 project files)

```
LicenseChecker.sln

src/license-detection/               (LicenseDetection.vcxproj - static library)
├── LicenseDetector.h/cpp            - main detector orchestrator
├── SLAPIDetector.h/cpp              - Windows SL API detection
├── WMIDetector.h/cpp                - WMI-based detection
├── RegistryDetector.h/cpp           - registry-based detection
├── WindowsSLMgrDetector.h/cpp       - slmgr.vbs-based detection
├── OfficeOSPPDetector.h/cpp         - Office (ospp.vbs) detection
├── DetectionLogger.h/cpp            - file logging
├── LicenseResult.h/cpp              - result data structure
├── LicenseInfo.h                    - plain data structs
└── LicenseStatusEnum.h              - LicenseStatus/KMSStatus enums

src/ui-native/                       (LicenseCheckerUI.vcxproj - the .exe)
├── main.cpp                         - WinMain entry point
├── MainWindow.h/cpp                 - tabbed Win32 window (Windows/Office/Settings), all controls
├── DetectionWorker.h/cpp            - background thread running the detection loop
├── ServerReporter.h/cpp             - reports results to the controller over WinHTTP
├── Localization.h/cpp               - Vietnamese/English strings
├── StartupLog.h/cpp                 - minimal startup/crash log
└── resource.h                       - app icon resource id
```

Both `.vcxproj` files already exist in the repo and are wired together by
`LicenseChecker.sln` - you don't hand-assemble this list to build, it's
here for reference (e.g. if extracting just this app into another repo).

## How to Build

See [BUILD_VS2022.md](BUILD_VS2022.md):
```batch
build-msbuild.bat
```
No Qt, no CMake, no qmake. Requires Visual Studio/Build Tools with the
"Desktop development with C++" workload and the v142 individual toolset
component.

## Build Output

```
x64\Release\
├── LicenseCheckerUI.exe    ← the whole app, ~500 KB
├── LicenseCheckerUI.pdb    ← debug symbols (optional, keep for crash analysis)
├── LicenseDetection.lib    ← intermediate static lib, not needed at runtime
└── LicenseDetection.pdb
```

## What to Ship

Just `LicenseCheckerUI.exe` - see [DEPLOY_GUIDE.md](DEPLOY_GUIDE.md). It
has no DLL dependencies beyond standard Windows system libraries every
Windows install already has, so there's nothing else to package.

## File Size Reference

| Component | Size |
|-----------|------|
| Source files (18 .h/.cpp) | a few hundred KB |
| `LicenseCheckerUI.exe` (Release) | ~500 KB |
| Everything needed to run it | just that one file |
