# License Checker - Build & Run Guide (Windows 7 SP1+)

## Prerequisites

### 1. **Visual Studio 2019 or 2022** (with C++ support)
   - Download: https://visualstudio.microsoft.com/downloads/
   - Select "Desktop development with C++"

### 2. **CMake 3.24+**
   - Download: https://cmake.org/download/
   - Add to PATH during installation

### 3. **Qt5 (LTS)**
   - Download: https://www.qt.io/download-open-source
   - Select Qt 5.15 LTS (supports Windows 7 SP1+)
   - Components to install:
     - MSVC 2019 64-bit (or 2022)
     - CMake tools
     - Qt Creator (optional, for debugging)

### 4. **Git** (optional)
   - Download: https://git-scm.com/download/win

## Build Steps

### Step 1: Set Environment Variables

Open PowerShell as Administrator and run:

```powershell
$env:Qt5_DIR = "C:\Qt\5.15.0\msvc2019_64"  # Adjust path to your Qt5 installation
$env:PATH = "$env:Qt5_DIR\bin;$env:PATH"
```

Or add to your system PATH permanently.

### Step 2: Create Build Directory

```bash
cd e:\License-checker
mkdir build
cd build
```

### Step 3: Configure with CMake

```bash
cmake -G "Visual Studio 16 2019" -A x64 -DCMAKE_PREFIX_PATH="C:\Qt\5.15.0\msvc2019_64" ..
```

**For Visual Studio 2022:**
```bash
cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:\Qt\5.15.0\msvc2019_64" ..
```

### Step 4: Build

```bash
cmake --build . --config Release
```

Output executables:
- `build\src\ui\Release\LicenseCheckerUI.exe` - Main UI Application
- `build\src\agent\Release\LicenseCheckerAgent.exe` - Background Service

## Run Application

### As Standalone App (for testing)

```bash
cd e:\License-checker\build\src\ui\Release
.\LicenseCheckerUI.exe
```

### Install As Windows Service (Agent)

```bash
# From administrator prompt
cd e:\License-checker\build\src\agent\Release
.\LicenseCheckerAgent.exe /install

# Start the service
net start LicenseCheckerAgent

# Check status
sc query LicenseCheckerAgent
```

### Uninstall Service

```bash
# From administrator prompt
net stop LicenseCheckerAgent
cd e:\License-checker\build\src\agent\Release
.\LicenseCheckerAgent.exe /uninstall
```

## Troubleshooting

### CMake can't find Qt5

```bash
# Use full path:
cmake -G "Visual Studio 16 2019" -A x64 ^
  -DCMAKE_PREFIX_PATH="C:\Qt\5.15.0\msvc2019_64" ^
  ..
```

### MSVC compiler not found

Ensure Visual Studio C++ workload is installed:
- Open Visual Studio Installer
- Modify Visual Studio
- Ensure "Desktop development with C++" is checked

### Service fails to start

Check logs in:
- `C:\ProgramData\LicenseCheckerAgent\` (if created)
- Event Viewer → Windows Logs → Application

### UI can't connect to Service

- Verify service is running: `sc query LicenseCheckerAgent`
- Check Named Pipe connection
- Ensure UI and Service have matching pipe name "LicenseChecker"

## Architecture

```
┌─────────────────────────────────────────────────────┐
│         Windows 7 SP1+ Machine                      │
├─────────────────────────────────────────────────────┤
│                                                     │
│  ┌──────────────────┐    ┌──────────────────────┐  │
│  │  License Checker │    │   LicenseChecker     │  │
│  │  UI Application  │◄──►│  Windows Service     │  │
│  │  (LicenseCheckerUI.exe)│  (LicenseCheckerAgent) │
│  └──────────────────┘    └──────────────────────┘  │
│         (Foreground)            (Background)       │
│                                                     │
│  Communication: Named Pipes (\.\pipe\LicenseChecker)
│  - UI sends: GetLicenseData, RequestImmediateCheck │
│  - Service sends: JSON responses with license data │
│                                                     │
└─────────────────────────────────────────────────────┘
```

## Features

✅ Windows License Detection (SL API → WMI → Registry fallback)
✅ Office License Detection
✅ KMS Server Detection
✅ System Tray Integration
✅ Auto-refresh every 5 minutes
✅ Manual "Check Now" button
✅ Named Pipe IPC (Service ↔ UI)
✅ Runs on Windows 7 SP1 and newer

## File Locations

- **Service logs**: `%LOCALAPPDATA%\LicenseCheckerAgent\license-detection.log`
- **Registry data**: `HKLM\SOFTWARE\LicenseCheckerAgent`
- **Executable**: Any location (portable)
