# Build Troubleshooting Guide

## Error: "cannot find -license_detection"

### Cause
The license_detection library hasn't been built yet, or the path is wrong.

### Solution

**Option 1: Clean Build (Recommended)**
```batch
cd build
mingw32-make distclean
cd ..
rmdir /s /q build
build-mingw.bat
```

**Option 2: Verify Qt5 Path**
```batch
set Qt5_DIR=C:\Qt\5.15.0\mingw81_64
echo %Qt5_DIR%
dir "%Qt5_DIR%\bin\qmake.exe"
```

Should show: `C:\Qt\5.15.0\mingw81_64\bin\qmake.exe`

**Option 3: Manual Build Order**
```batch
set Qt5_DIR=C:\Qt\5.15.0\mingw81_64
set PATH=%Qt5_DIR%\bin;%PATH%

REM Clean
rmdir /s /q build

REM Create fresh build dir
mkdir build
cd build

REM Configure
qmake -r -spec win32-g++ ..\LicenseChecker.pro

REM Build
mingw32-make -j4

cd ..
```

---

## Error: "qmake: command not found"

### Cause
Qt5 bin directory not in PATH

### Solution

**Check qmake location:**
```batch
where qmake
```

If not found, add to PATH:
```batch
set Qt5_DIR=C:\Qt\5.15.0\mingw81_64
set PATH=%Qt5_DIR%\bin;%PATH%
qmake --version
```

**Permanent PATH (Windows):**
1. Press `Win + Pause`
2. Click "Advanced system settings"
3. Click "Environment Variables"
4. Add `C:\Qt\5.15.0\mingw81_64\bin` to PATH
5. Restart terminal/IDE

---

## Error: "mingw32-make: command not found"

### Cause
MinGW not in PATH or not included with Qt5

### Solution

**Option 1: Use Qt5's MinGW**
```batch
set Qt5_DIR=C:\Qt\5.15.0\mingw81_64
set PATH=%Qt5_DIR%\bin;%PATH%
mingw32-make --version
```

**Option 2: Reinstall Qt5 with MinGW**
- Download Qt5.15 LTS
- Select "MinGW 8.1 64-bit" component
- Re-run installer

**Option 3: Install MinGW Separately**
- Download: https://sourceforge.net/projects/mingw-w64/
- Add `C:\mingw64\bin` to PATH

---

## Error: "undefined reference to `LicenseDetector::Detect()'"

### Cause
License detection library not linked properly

### Solution

**Check .pro files have correct LIBS:**
```pro
# In src/agent/Agent.pro and src/ui/UI.pro:
LIBS += -L$$OUT_PWD/../license-detection -llicense_detection
LIBS += -L$$OUT_PWD/../common -lcommon
```

**Clean rebuild:**
```batch
cd build
mingw32-make distclean
qmake -r -spec win32-g++ ..\LicenseChecker.pro
mingw32-make -j4
```

---

## Error: "C:\Qt\5.15.0\mingw81_64\bin\g++.exe: error: createprocess"

### Cause
Path too long or special characters in Qt installation path

### Solution

**Move Qt to shorter path:**
```batch
REM Move from: C:\Qt\5.15.0\mingw81_64
REM To: C:\Qt5
xcopy "C:\Qt\5.15.0" "C:\Qt5" /E /I
set Qt5_DIR=C:\Qt5
```

**Or use shorter build path:**
```batch
cd \
mkdir lc_build
cd lc_build
qmake -spec win32-g++ C:\License-checker\LicenseChecker.pro
mingw32-make
```

---

## Error: "fatal error: cannot open source file 'LicenseDetector.h'"

### Cause
Include path not set correctly in .pro file

### Solution

**Verify includes in .pro:**
```pro
INCLUDEPATH += $$PWD \
               $$PWD/../license-detection \
               $$PWD/../common
```

**Check file exists:**
```batch
dir src\license-detection\LicenseDetector.h
```

---

## Build is Slow

### Solution

**Use parallel jobs:**
```batch
mingw32-make -j8
```

Use number = your CPU cores (check `wmic logicalprocessor get`)

**Skip debug info (Release build):**
```bash
qmake -r -spec win32-g++ CONFIG+=release ..\LicenseChecker.pro
mingw32-make -j8
```

---

## Executable Won't Run

### Error: "DLL not found"

**Solution:**
Copy Qt5 runtime DLLs to executable directory:
```batch
copy "C:\Qt\5.15.0\mingw81_64\bin\Qt5Core.dll" build\src\ui\
copy "C:\Qt\5.15.0\mingw81_64\bin\Qt5Gui.dll" build\src\ui\
copy "C:\Qt\5.15.0\mingw81_64\bin\Qt5Widgets.dll" build\src\ui\
copy "C:\Qt\5.15.0\mingw81_64\bin\Qt5Network.dll" build\src\ui\
copy "C:\Qt\5.15.0\mingw81_64\bin\libgcc_s_seh-1.dll" build\src\ui\
copy "C:\Qt\5.15.0\mingw81_64\bin\libstdc++-6.dll" build\src\ui\
copy "C:\Qt\5.15.0\mingw81_64\bin\libwinpthread-1.dll" build\src\ui\
```

**Or add Qt bin to PATH:**
```batch
set PATH=C:\Qt\5.15.0\mingw81_64\bin;%PATH%
build\src\ui\LicenseCheckerUI.exe
```

---

## Service Won't Install/Start

### Error: "Access Denied"

**Solution:** Run as Administrator
```batch
REM Run cmd.exe as Administrator first, then:
build\src\agent\LicenseCheckerAgent.exe /install
net start LicenseCheckerAgent
```

### Error: "The service did not respond"

**Solution:**
1. Stop service: `net stop LicenseCheckerAgent`
2. Check Windows Event Viewer for errors
3. Reinstall:
```batch
build\src\agent\LicenseCheckerAgent.exe /uninstall
build\src\agent\LicenseCheckerAgent.exe /install
net start LicenseCheckerAgent
```

---

## UI Doesn't Show License Data

### Cause
Service not running

### Solution

**Check service status:**
```batch
sc query LicenseCheckerAgent
```

**Start service:**
```batch
net start LicenseCheckerAgent
```

**Or run in standalone mode** (for testing):
```batch
REM Service not needed for UI testing, but data will be empty/cached
build\src\ui\LicenseCheckerUI.exe
```

---

## Still Having Issues?

### Debug Info

**Show full build output:**
```batch
cd build
mingw32-make clean
qmake -r -spec win32-g++ ..\LicenseChecker.pro
mingw32-make
```

Copy error messages and check:
1. Qt5 installation is complete
2. MinGW is included
3. All paths are correct
4. No special characters in paths

### Get Help

Provide:
1. Error message (full output)
2. Qt5 path: `echo %Qt5_DIR%`
3. OS version: `ver`
4. Build command used
