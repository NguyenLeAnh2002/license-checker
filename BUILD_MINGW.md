# License Checker - Build Guide (MinGW/Qt5)

Build the License Checker for **Windows 7 SP1+** using MinGW and Qt5 with qmake.

## Prerequisites

### 1. **Qt5 with MinGW** (Recommended)
   - Download: https://www.qt.io/download-open-source
   - Select **Qt 5.15 LTS** with **MinGW 8.1** (64-bit)
   - Installation path: `C:\Qt\5.15.0\mingw81_64`

### 2. **MinGW** (if not included with Qt)
   - Included with Qt5 installer
   - Or download standalone: https://sourceforge.net/projects/mingw-w64/

### 3. **Git** (optional)
   - Download: https://git-scm.com/download/win

## Quick Start

### Step 1: Set Environment Variable

**Option A - Batch Script:**
```batch
set Qt5_DIR=C:\Qt\5.15.0\mingw81_64
build-mingw.bat
```

**Option B - PowerShell:**
```powershell
$env:Qt5_DIR = "C:\Qt\5.15.0\mingw81_64"
.\build-mingw.ps1
```

**Option C - Manual (Command Prompt):**
```batch
set Qt5_DIR=C:\Qt\5.15.0\mingw81_64
set PATH=%Qt5_DIR%\bin;%PATH%
mkdir build
cd build
qmake -r -spec win32-g++ ..\LicenseChecker.pro
mingw32-make -j4
```

## Step-by-Step Manual Build

### Step 1: Create Build Directory
```batch
mkdir build
cd build
```

### Step 2: Run qmake
```batch
set Qt5_DIR=C:\Qt\5.15.0\mingw81_64
%Qt5_DIR%\bin\qmake -r -spec win32-g++ ..\LicenseChecker.pro
```

### Step 3: Build with MinGW
```batch
mingw32-make -j4
```

The `-j4` flag uses 4 parallel jobs for faster compilation. Adjust based on your CPU cores.

### Step 4: Build Output
- **UI Application:** `src\ui\LicenseCheckerUI.exe`
- **Service/Agent:** `src\agent\LicenseCheckerAgent.exe`

## Run Application

### Standalone UI (Testing)
```batch
src\ui\LicenseCheckerUI.exe
```

### Install as Windows Service
```batch
REM Run as Administrator
src\agent\LicenseCheckerAgent.exe /install
net start LicenseCheckerAgent
```

### Check Service Status
```batch
sc query LicenseCheckerAgent
```

### Stop Service
```batch
net stop LicenseCheckerAgent
```

### Uninstall Service
```batch
REM Run as Administrator
net stop LicenseCheckerAgent
src\agent\LicenseCheckerAgent.exe /uninstall
```

## Project Structure

```
LicenseChecker.pro              (Root project file)
├── src/license-detection/
│   └── LicenseDetection.pro    (Static library)
├── src/common/
│   └── Common.pro              (Static library)
├── src/agent/
│   └── Agent.pro               (Console application)
└── src/ui/
    └── UI.pro                  (GUI application)
```

## Build Configuration

### Release Build (Optimized)
```bash
qmake -r -spec win32-g++ CONFIG+=release ..\LicenseChecker.pro
mingw32-make
```

### Debug Build (With symbols)
```bash
qmake -r -spec win32-g++ CONFIG+=debug ..\LicenseChecker.pro
mingw32-make
```

### Both Debug and Release
```bash
qmake -r -spec win32-g++ ..\LicenseChecker.pro
mingw32-make
```

## Troubleshooting

### qmake: command not found
- Add Qt5 bin directory to PATH
- Or use full path: `C:\Qt\5.15.0\mingw81_64\bin\qmake.exe`

### mingw32-make: command not found
- Qt5 MinGW installation should include make
- Or install standalone MinGW and add to PATH

### Build errors with Qt5
- Ensure Qt5.15 LTS is installed (not Qt6)
- Verify spec is `win32-g++` not `win32-msvc`

### Service won't start
- Run Command Prompt as Administrator
- Check Windows Event Viewer for errors
- Verify Named Pipe: `\\.\pipe\LicenseChecker`

### UI can't connect to Service
- Ensure service is running: `sc query LicenseCheckerAgent`
- Check Windows firewall (shouldn't block local pipes)

## Performance Tips

### Faster Builds
```batch
REM Use all CPU cores
mingw32-make -j8
```

### Smaller Binaries
```batch
REM Strip debug symbols (release only)
strip src\ui\LicenseCheckerUI.exe
strip src\agent\LicenseCheckerAgent.exe
```

### Faster Incremental Builds
```batch
REM Only rebuild changed files
mingw32-make
```

## .pro File Reference

### LicenseChecker.pro (Root)
```pro
TEMPLATE = subdirs          # Multi-project layout
CONFIG += ordered           # Build in order

SUBDIRS = \
    src/license-detection/LicenseDetection.pro \
    src/common/Common.pro \
    src/agent/Agent.pro \
    src/ui/UI.pro
```

### src/license-detection/LicenseDetection.pro
```pro
TEMPLATE = lib              # Build as static library
CONFIG += staticlib c++17
TARGET = license_detection
SOURCES += ...              # Source files
HEADERS += ...              # Header files
```

### src/ui/UI.pro
```pro
TEMPLATE = app              # Build as application
CONFIG += c++17
QT += core gui widgets network
TARGET = LicenseCheckerUI
SOURCES += ...
HEADERS += ...
LIBS += -L../common -lcommon -L../license-detection -llicense_detection
```

## Advanced Options

### Cross-Compile for 32-bit
```batch
qmake -r -spec win32-g++ CONFIG+=x86 ..\LicenseChecker.pro
mingw32-make
```

### Static Linking
```batch
REM Build with static Qt libraries (requires static Qt5 build)
qmake -r -spec win32-g++ CONFIG+=static ..\LicenseChecker.pro
mingw32-make
```

### Clean Build
```batch
mingw32-make clean
qmake -r -spec win32-g++ ..\LicenseChecker.pro
mingw32-make
```

## System Requirements

- **OS:** Windows 7 SP1 or newer
- **RAM:** 512 MB minimum
- **Disk:** 100 MB for Qt5 + 50 MB for project
- **Compiler:** MinGW 8.1+ (included with Qt5)

## Next Steps

1. ✅ Build project using build-mingw.bat or build-mingw.ps1
2. ✅ Test UI: `src\ui\LicenseCheckerUI.exe`
3. ✅ Install service: `src\agent\LicenseCheckerAgent.exe /install`
4. ✅ Verify service: `sc query LicenseCheckerAgent`
5. ✅ Check license status in UI application

## Support

For issues or questions, check:
- Windows Event Viewer (Application logs)
- Service status: `sc query LicenseCheckerAgent`
- Named Pipe connection test in UI
