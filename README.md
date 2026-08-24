# License Checker

A Windows desktop application for monitoring Windows and Office license status. Supports **Windows 7 SP1 and newer**.

## Features

✅ **License Detection**
- Windows license status (Legitimate, Cracked, Not Licensed, Unable to Determine)
- Office license status
- KMS server detection
- Automatic 5-minute interval checks
- Manual "Check Now" trigger

✅ **User Interface**
- Tabbed interface: Windows License, Office License, Settings
- Color-coded status badges
- Vietnamese/English language switcher (defaults to Vietnamese)
- System/host information display (hostname, machine GUID, department)

✅ **Architecture**
- Single native Win32 application - no background service, no IPC
- Detection runs on a worker thread inside the same process as the UI
- Self-reports results to a controller server (see
  `AGENT_SERVER_PROTOCOL_REAL.md`) - the controller address comes from a
  trailer block stamped into the built exe (see
  `license_checker_server/internal/checkerstamp` in the sibling repo) or an
  optional `agent_config.json` next to the executable

✅ **Cross-Platform License Detection**
- SL API (Windows Service) → highest priority
- WMI queries → secondary fallback
- Registry inspection → last resort

## Quick Start

**Prerequisites:** Visual Studio 2022 (or standalone "Build Tools for
Visual Studio 2022") with the "Desktop development with C++" workload,
plus the individual component "MSVC v142 - VS 2019 C++ x64/x86 build
tools". **No Qt, no CMake, no qmake needed** - everything this project
links against ships with the Windows SDK that workload installs.

**Build:**
```bash
build-msbuild.bat
```
or manually:
```bash
MSBuild LicenseChecker.sln /p:Configuration=Release /p:Platform=x64
```

See [BUILD_VS2022.md](BUILD_VS2022.md) for detailed instructions,
prerequisites, and troubleshooting.

**Run:**
```bash
x64\Release\LicenseCheckerUI.exe
```
It's a standalone GUI app - just run it directly, there's nothing to
install or start as a service.

> Other build docs/scripts in this repo (`LicenseChecker.pro`,
> `CMakeLists.txt`, `build.bat`, `build-mingw.bat`/`.ps1`,
> `BUILD_GUIDE.md`, `BUILD_MINGW.md`, `QUICK_BUILD.md`) describe an older
> Qt-based UI + separate Windows-service agent architecture that has since
> been replaced by the single native app described here, and no longer
> match this repository's source tree - they're marked outdated in place
> and kept only for history.

## Architecture

### Components

```
src/
├── license-detection/    # Core license detection logic (static library)
│   ├── LicenseDetector.cpp        - Main detector orchestrator
│   ├── SLAPIDetector.cpp          - Windows SL API detection
│   ├── WMIDetector.cpp            - WMI-based detection
│   ├── RegistryDetector.cpp       - Registry-based detection
│   ├── WindowsSLMgrDetector.cpp   - slmgr.vbs-based detection
│   ├── OfficeOSPPDetector.cpp     - Office (ospp.vbs) detection
│   ├── DetectionLogger.cpp        - File logging
│   └── LicenseResult.cpp          - Data structure for results
│
└── ui-native/            # The application itself (single .exe)
    ├── main.cpp              - WinMain entry point
    ├── MainWindow.cpp        - Tabbed Win32 window, all UI controls
    ├── DetectionWorker.cpp   - Background thread running the detection loop
    ├── ServerReporter.cpp    - Reports results to the controller over WinHTTP
    ├── Localization.cpp      - Vietnamese/English strings
    └── StartupLog.cpp        - Minimal startup/crash log
```

### Data Flow

```
DetectionWorker thread                    MainWindow (UI thread)
│                                          │
├─ Detect License                         │
│  (SL API → WMI → Registry)              │
│                                          │
├─ ResultCallback ─────────────────────────►  update tabs/badges
│                                          │
├─ ServerReporter::SendReport() ──HTTP──►  controller (license_checker_server)
│  POST /api/report
```

There is no named-pipe IPC and no separate service process - detection,
UI, and reporting all live in one `LicenseCheckerUI.exe`.

## File Structure

```
license-checker/
├── LicenseChecker.sln            # Visual Studio solution (the real build)
├── BUILD_VS2022.md               # Current build instructions
├── build-msbuild.bat             # Build script (MSBuild, no Qt)
├── AGENT_SERVER_PROTOCOL_REAL.md # Wire protocol with the controller
├── README.md                     # This file
│
├── src/
│   ├── license-detection/
│   │   ├── LicenseDetection.vcxproj
│   │   └── ... (see Architecture above)
│   │
│   └── ui-native/
│       ├── LicenseCheckerUI.vcxproj
│       └── ... (see Architecture above)
│
└── x64/                           # Build output (after building)
    └── Release/
        └── LicenseCheckerUI.exe
```

## System Requirements

- **OS:** Windows 7 SP1 or newer
- **RAM:** 256 MB minimum
- **Disk:** 50 MB minimum
- **Privileges:** Administrator recommended (SL API/WMI detection is more complete when elevated)

## Troubleshooting

### UI won't start
- Check `x64\Release\license_checker_startup.log` and `license-detection.log` next to the exe
- Check Windows Event Viewer for errors

### License detection fails / incomplete
- Run as Administrator (required for full SL API and WMI access)
- Check `license-detection.log` next to the exe
- Verify Windows is not in trial period

### Not reporting to the controller
- Check `license-detection.log` for `ServerReporter:` lines - it logs
  whether it found an embedded address, an `agent_config.json`, or neither
- See `AGENT_SERVER_PROTOCOL_REAL.md` and the sibling `license_checker_server`
  repo's `internal/checkerstamp` package for how the controller address gets
  embedded into the exe - and that repo's `docs/checker-stamp-and-signing.md`
  for why re-stamping doesn't break this exe's Authenticode signature

## License

This project is provided as-is for enterprise license compliance monitoring.

## Support

See [BUILD_VS2022.md](BUILD_VS2022.md) for build instructions and
`AGENT_SERVER_PROTOCOL_REAL.md` for the reporting protocol.
