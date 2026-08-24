# License Checker

A Windows desktop application for monitoring Windows and Office license status with system tray integration. Supports **Windows 7 SP1 and newer**.

## Features

✅ **License Detection**
- Windows license status (Legitimate, Cracked, Not Licensed, Unable to Determine)
- Office license status
- KMS server detection
- Automatic 5-minute interval checks
- Manual "Check Now" trigger

✅ **User Interface**
- Tabbed interface: Windows License, Office License, Settings
- Color-coded status badges (Green/Red/Yellow/Gray)
- System information display
- System tray integration with quick-status popup
- Minimize to tray functionality

✅ **Architecture**
- Service/Agent runs in background (Windows Service)
- UI application displays license data
- Named Pipe IPC (inter-process communication) for real-time updates
- Registry fallback for data persistence

✅ **Cross-Platform License Detection**
- SL API (Windows Service) → highest priority
- WMI queries → secondary fallback
- Registry inspection → last resort

## Quick Start

### Option 1: MinGW/Qt5 (Recommended for Windows 7)

**Install Dependencies:**
- Qt5.15 LTS with MinGW 8.1 (download: https://www.qt.io/download-open-source)

**Build (easiest):**
```bash
set Qt5_DIR=C:\Qt\5.15.0\mingw81_64
build-mingw.bat
```

**Or PowerShell:**
```powershell
.\build-mingw.ps1 -Qt5Path "C:\Qt\5.15.0\mingw81_64"
```

**Or manual:**
```bash
set Qt5_DIR=C:\Qt\5.15.0\mingw81_64
set PATH=%Qt5_DIR%\bin;%PATH%
mkdir build && cd build
qmake -r -spec win32-g++ ..\LicenseChecker.pro
mingw32-make -j4
```

See [BUILD_MINGW.md](BUILD_MINGW.md) for detailed instructions.

### Option 2: CMake + MSVC (Visual Studio)

**Install Dependencies:**
- Visual Studio 2019/2022 (with C++ support)
- CMake 3.24+
- Qt5.15 LTS

**Build:**
```bash
set Qt5_DIR=C:\Qt\5.15.0\msvc2019_64
build.bat
```

See [BUILD_GUIDE.md](BUILD_GUIDE.md) for detailed instructions.

### 3. Run

**UI Application (standalone):**
```bash
build\src\ui\Release\LicenseCheckerUI.exe
```

**Install as Windows Service:**
```bash
# Run as Administrator
build\src\agent\Release\LicenseCheckerAgent.exe /install
net start LicenseCheckerAgent
```

## Architecture

### Components

```
src/
├── license-detection/    # Core license detection logic
│   ├── LicenseDetector.cpp      - Main detector orchestrator
│   ├── SLAPIDetector.cpp        - Windows SL API detection
│   ├── WMIDetector.cpp          - WMI-based detection
│   ├── RegistryDetector.cpp     - Registry-based detection
│   └── LicenseResult.cpp        - Data structure for results
│
├── agent/               # Background service
│   ├── LicenseDetectionWorker.cpp   - Periodic detection worker
│   ├── ServiceMain.cpp              - Service entry point
│   └── ServiceInstaller.cpp         - Service install/uninstall
│
├── ui/                  # Desktop application
│   ├── MainWindow.cpp       - Main tabbed interface
│   ├── SystemTrayIcon.cpp   - System tray integration
│   ├── LicenseDataModel.cpp - Data model for UI
│   └── main.cpp            - Application entry point
│
└── common/              # Shared utilities
    ├── NamedPipeServer.cpp  - Service-side IPC
    ├── NamedPipeClient.cpp  - UI-side IPC
    └── SharedLicenseDataWriter.cpp - Registry writer
```

### Communication Flow

```
Service (Background)                UI (Foreground)
│                                   │
├─ Detect License                   │
│  (SL API → WMI → Registry)        │
│                                   │
├─ NamedPipeServer                  ├─ NamedPipeClient
│  Listen on: \\.\pipe\LicenseChecker
│                                   │
│                   ◄───── GetLicenseData ─────┤
│                                   │
│ Send JSON Response ────────────────►
│ {status, data{windows, office}}    │
│                                   │
│                   ◄─ RequestImmediateCheck ──┤
│                                   │
├─ Queue immediate detection        │
│  (Run Detect cycle again)          │
│                                   │
```

## File Structure

```
License-checker/
├── CMakeLists.txt              # Root CMake configuration
├── BUILD_GUIDE.md              # Detailed build instructions
├── build.bat                   # Batch build script
├── build.ps1                   # PowerShell build script
├── README.md                   # This file
│
├── src/
│   ├── CMakeLists.txt
│   ├── ui/
│   │   ├── CMakeLists.txt
│   │   ├── MainWindow.h/cpp
│   │   ├── SystemTrayIcon.h/cpp
│   │   ├── LicenseDataModel.h/cpp
│   │   └── main.cpp
│   │
│   ├── agent/
│   │   ├── CMakeLists.txt
│   │   ├── LicenseDetectionWorker.h/cpp
│   │   ├── ServiceMain.cpp
│   │   └── ServiceInstaller.cpp
│   │
│   ├── license-detection/
│   │   ├── CMakeLists.txt
│   │   ├── LicenseDetector.h/cpp
│   │   ├── SLAPIDetector.h/cpp
│   │   ├── WMIDetector.h/cpp
│   │   ├── RegistryDetector.h/cpp
│   │   ├── LicenseResult.h/cpp
│   │   ├── LicenseStatusEnum.h
│   │   └── DetectionLogger.h/cpp
│   │
│   └── common/
│       ├── CMakeLists.txt
│       ├── NamedPipeServer.h/cpp
│       ├── NamedPipeClient.h/cpp
│       └── SharedLicenseDataWriter.h/cpp
│
└── build/                      # Build output (after building)
    └── src/
        ├── ui/Release/LicenseCheckerUI.exe
        └── agent/Release/LicenseCheckerAgent.exe
```

## Data Format

### License Status Enum
```
0 = Legitimate
1 = Cracked
2 = Not Licensed
3 = Unable to Determine
```

### KMS Status Enum
```
0 = Not KMS
1 = KMS Detected
2 = KMS Not Found
3 = Error
```

### IPC Message Format

**Request (UI → Service):**
```
GetLicenseData
RequestImmediateCheck
```

**Response (Service → UI):**
```json
{
  "status": "success",
  "data": {
    "windows": {
      "edition": "Windows 10 Pro",
      "licenseStatus": 0,
      "kmsStatus": 1,
      "kmsServer": "kms.corp.local",
      "lastDetected": "2026-08-06T11:05:00Z"
    },
    "office": {
      "edition": "Office 365 ProPlus",
      "licenseStatus": 0,
      "kmsStatus": 1,
      "kmsServer": "kms.corp.local",
      "lastDetected": "2026-08-06T11:05:00Z"
    }
  }
}
```

## System Requirements

- **OS:** Windows 7 SP1 or newer
- **RAM:** 256 MB minimum
- **Disk:** 50 MB minimum
- **Privileges:** Administrator (for service installation only)

## Troubleshooting

### UI won't start
- Ensure Qt5 runtime libraries are available
- Check Windows Event Viewer for errors
- Verify Named Pipe connection (Service must be running)

### Service won't install
- Run installer as Administrator
- Check Windows Event Viewer → Application logs
- Ensure no other instance is running

### License detection fails
- Run as Administrator (required for SL API and WMI)
- Check Windows Event Viewer for service logs
- Verify Windows is not in trial period

## License

This project is provided as-is for enterprise license compliance monitoring.

## Support

See `BUILD_GUIDE.md` for detailed build and deployment instructions.
