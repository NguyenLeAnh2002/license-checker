# License Checker UI - Complete Package

## File Cần Thiết Để Build Trên Máy Khác

### Root Files
```
UIOnly.pro                  # Main build configuration
build-ui-only.bat          # Build script
build-ui-only.ps1          # PowerShell build script (optional)
```

### UI Source Files (`src/ui/`)
```
src/ui/
├── main.cpp               # Application entry point
├── MainWindow.h           # Main window header
├── MainWindow.cpp         # Main window implementation
├── SystemTrayIcon.h       # System tray header
├── SystemTrayIcon.cpp     # System tray implementation
├── LicenseDataModel.h     # Data model header
└── LicenseDataModel.cpp   # Data model implementation
```

### Common/IPC Files (`src/common/`)
```
src/common/
├── NamedPipeClient.h      # Named Pipe client header
└── NamedPipeClient.cpp    # Named Pipe client implementation
```

## Total Files: 11 files

## How to Use on Another Machine

### Step 1: Copy Files
Copy these exact files to new machine:
```
C:\YourPath\License-checker\
├── UIOnly.pro
├── build-ui-only.bat
├── build-ui-only.ps1
└── src/
    ├── ui/
    │   ├── main.cpp
    │   ├── MainWindow.h
    │   ├── MainWindow.cpp
    │   ├── SystemTrayIcon.h
    │   ├── SystemTrayIcon.cpp
    │   ├── LicenseDataModel.h
    │   └── LicenseDataModel.cpp
    └── common/
        ├── NamedPipeClient.h
        └── NamedPipeClient.cpp
```

### Step 2: Install Qt5 on New Machine
- Download Qt5.15 LTS: https://www.qt.io/download-open-source
- Select MinGW 8.1 64-bit
- Install to: `C:\Qt\5.15.0\mingw81_64`

### Step 3: Build
```batch
set Qt5_DIR=C:\Qt\5.15.0\mingw81_64
cd C:\YourPath\License-checker
build-ui-only.bat
```

### Step 4: Run
```batch
build\release\LicenseCheckerUI.exe
```

## File Descriptions

| File | Purpose |
|------|---------|
| **UIOnly.pro** | qmake project file - defines build configuration |
| **main.cpp** | Creates Qt application and MainWindow |
| **MainWindow.h/cpp** | Main GUI with 3 tabs (Windows License, Office License, Settings) |
| **SystemTrayIcon.h/cpp** | System tray integration with popup menu |
| **LicenseDataModel.h/cpp** | Data structures for license information |
| **NamedPipeClient.h/cpp** | Communication with service (or mock data) |
| **build-ui-only.bat** | Windows batch build script |
| **build-ui-only.ps1** | Windows PowerShell build script |

## Dependencies

### External
- Qt5.15 LTS (with MinGW 8.1)
- Windows 7 SP1 or newer

### Internal
- All code is self-contained in these 11 files
- No external libraries required

## Build Output

After successful build:
```
build\release\LicenseCheckerUI.exe  (≈ 5-10 MB with Qt5 DLLs)
```

## Standalone Executable

To make it portable, copy Qt5 DLLs:
```batch
mkdir dist
copy build\release\LicenseCheckerUI.exe dist\
copy C:\Qt\5.15.0\mingw81_64\bin\Qt5Core.dll dist\
copy C:\Qt\5.15.0\mingw81_64\bin\Qt5Gui.dll dist\
copy C:\Qt\5.15.0\mingw81_64\bin\Qt5Widgets.dll dist\
copy C:\Qt\5.15.0\mingw81_64\bin\Qt5Network.dll dist\
copy C:\Qt\5.15.0\mingw81_64\bin\libgcc_s_seh-1.dll dist\
copy C:\Qt\5.15.0\mingw81_64\bin\libstdc++-6.dll dist\
copy C:\Qt\5.15.0\mingw81_64\bin\libwinpthread-1.dll dist\
```

Then you can run:
```batch
dist\LicenseCheckerUI.exe
```

On any Windows 7+ machine without Qt5 installed.

## File Size Reference

| Component | Size |
|-----------|------|
| Source files | ~100 KB |
| Build output (exe only) | ~500 KB |
| With Qt5 DLLs | ~100 MB |
| Compressed (7z) | ~30 MB |
