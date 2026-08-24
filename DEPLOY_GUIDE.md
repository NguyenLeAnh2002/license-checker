# Deploy License Checker UI to Another Machine

## Option A: Copy Source Files (Recommended)

### On Source Machine (Current Machine):

**Step 1: Create Package**
```batch
create-package.bat
```

Output: `C:\LicenseChecker-UI-Package\`

**Step 2: Copy to USB/Network**
- Copy `C:\LicenseChecker-UI-Package\` to USB drive or network share
- Or compress: `Right-click → Send to → Compressed (zipped) folder`

### On Target Machine (New Machine):

**Step 1: Install Qt5**
- Download: https://www.qt.io/download-open-source
- Select Qt 5.15 LTS
- Select MinGW 8.1 64-bit
- Install to: `C:\Qt\5.15.0\mingw81_64`

**Step 2: Copy Package**
- Copy `LicenseChecker-UI-Package\` to `C:\LicenseChecker-UI`

**Step 3: Build**
```batch
set Qt5_DIR=C:\Qt\5.15.0\mingw81_64
cd C:\LicenseChecker-UI
build-ui-only.bat
```

**Step 4: Run**
```batch
build\release\LicenseCheckerUI.exe
```

---

## Option B: Manual File Copy

### Files to Copy (11 files):

**Root level:**
- `UIOnly.pro`
- `build-ui-only.bat`
- `build-ui-only.ps1`

**src/ui/ (7 files):**
- `main.cpp`
- `MainWindow.h`
- `MainWindow.cpp`
- `SystemTrayIcon.h`
- `SystemTrayIcon.cpp`
- `LicenseDataModel.h`
- `LicenseDataModel.cpp`

**src/common/ (2 files):**
- `NamedPipeClient.h`
- `NamedPipeClient.cpp`

### Copy Structure:
```
YourProject\
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

---

## Option C: Standalone Executable (No Qt5 Installation)

### Prerequisites:
- Already built on source machine with all Qt5 DLLs

### On Source Machine:

**Step 1: Create Distribution Folder**
```batch
mkdir dist
copy build\release\LicenseCheckerUI.exe dist\

REM Copy Qt5 DLLs
copy C:\Qt\5.15.0\mingw81_64\bin\Qt5Core.dll dist\
copy C:\Qt\5.15.0\mingw81_64\bin\Qt5Gui.dll dist\
copy C:\Qt\5.15.0\mingw81_64\bin\Qt5Widgets.dll dist\
copy C:\Qt\5.15.0\mingw81_64\bin\Qt5Network.dll dist\
copy C:\Qt\5.15.0\mingw81_64\bin\libgcc_s_seh-1.dll dist\
copy C:\Qt\5.15.0\mingw81_64\bin\libstdc++-6.dll dist\
copy C:\Qt\5.15.0\mingw81_64\bin\libwinpthread-1.dll dist\
```

**Step 2: Compress**
```batch
REM Create ZIP
Right-click dist → Send to → Compressed (zipped) folder
```

### On Target Machine:

**Step 1: Extract**
- Extract `dist.zip` to anywhere

**Step 2: Run**
```batch
LicenseCheckerUI.exe
```

No Qt5 installation needed!

---

## Verification Checklist

- [ ] Qt5.15 LTS installed with MinGW 8.1
- [ ] All 11 source files copied
- [ ] Folder structure matches (src/ui/, src/common/)
- [ ] `UIOnly.pro` exists in root
- [ ] `build-ui-only.bat` exists in root
- [ ] No spaces or special characters in path
- [ ] Windows 7 SP1 or newer
- [ ] At least 1 GB free disk space

---

## Troubleshooting on Target Machine

### "qmake: command not found"
```batch
set Qt5_DIR=C:\Qt\5.15.0\mingw81_64
set PATH=%Qt5_DIR%\bin;%PATH%
```

### "mingw32-make: command not found"
Qt5 installation missing MinGW component. Reinstall Qt5.

### Build succeeds but EXE won't run
Missing Qt5 DLLs. Either:
1. Add Qt5 bin to PATH:
   ```batch
   set PATH=C:\Qt\5.15.0\mingw81_64\bin;%PATH%
   LicenseCheckerUI.exe
   ```
2. Or copy DLLs to same folder as EXE

### "Cannot find -license_detection"
Check that `NamedPipeClient.h/cpp` are in `src/common/` folder.

---

## File Size Reference

| Item | Size |
|------|------|
| Source files (11 files) | ~100 KB |
| Built executable only | ~500 KB |
| With Qt5 DLLs | ~100 MB |
| Compressed package | ~30 MB |

---

## Network Deployment

### Share via Cloud:
1. Compress `dist` folder (standalone option)
2. Upload to OneDrive/Google Drive/Dropbox
3. Share link on target machine
4. Download and run

### Share via Network Drive:
```batch
REM On source machine
xcopy dist Z:\LicenseChecker-UI /E /I

REM On target machine
Z:\LicenseChecker-UI\LicenseCheckerUI.exe
```

---

## Quick Reference Commands

**Source Machine - Create Package:**
```batch
create-package.bat
```

**Target Machine - Build from Source:**
```batch
set Qt5_DIR=C:\Qt\5.15.0\mingw81_64
build-ui-only.bat
build\release\LicenseCheckerUI.exe
```

**Target Machine - Run Standalone:**
```batch
LicenseCheckerUI.exe
```

---

## Support

For issues, check:
1. Qt5 installation: `%Qt5_DIR%\bin\qmake.exe` exists
2. Build errors: `build\release\` folder exists
3. Runtime errors: All Qt5 DLLs in same folder as EXE
