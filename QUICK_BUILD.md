# Quick Build Reference

> ⚠️ **OUTDATED.** Everything below describes the old Qt5-based build
> (`src/ui`, `src/agent`, `src/common`), which no longer exist in this
> repository - none of it will work. The current build needs no Qt at
> all: see [BUILD_VS2022.md](BUILD_VS2022.md) and run `build-msbuild.bat`.

## MinGW + Qt5 (Windows 7+ Compatible) ✅

### One-liner Build:
```batch
set Qt5_DIR=C:\Qt\5.15.0\mingw81_64 && build-mingw.bat
```

### Or PowerShell:
```powershell
$env:Qt5_DIR = "C:\Qt\5.15.0\mingw81_64"; .\build-mingw.ps1
```

### Run UI:
```batch
build\src\ui\LicenseCheckerUI.exe
```

### Install Service (Admin):
```batch
build\src\agent\LicenseCheckerAgent.exe /install
net start LicenseCheckerAgent
```

---

## Visual Studio + CMake (Windows 10+)

### One-liner Build:
```batch
set Qt5_DIR=C:\Qt\5.15.0\msvc2019_64 && build.bat
```

### Or PowerShell:
```powershell
$env:Qt5_DIR = "C:\Qt\5.15.0\msvc2019_64"; .\build.ps1
```

### Run UI:
```batch
build\src\ui\Release\LicenseCheckerUI.exe
```

---

## Environment Setup

### Check Qt5 Installation
```batch
where qmake          REM Should show C:\Qt\5.15.0\mingw81_64\bin\qmake.exe
```

### Set Path Permanently (Windows)
1. Open System Properties → Environment Variables
2. Add `C:\Qt\5.15.0\mingw81_64\bin` to PATH
3. Restart terminal

---

## Troubleshooting

| Error | Solution |
|-------|----------|
| `qmake: command not found` | Add Qt5\bin to PATH |
| `mingw32-make: command not found` | Ensure MinGW is installed with Qt5 |
| Service won't start | Run installer as Administrator |
| UI won't run | Ensure Qt5 runtime DLLs are in PATH |

---

## Build Outputs

**After successful build:**
- `build\src\ui\LicenseCheckerUI.exe` - Desktop UI application
- `build\src\agent\LicenseCheckerAgent.exe` - Background service

---

## Development Mode

### Edit Code + Rebuild:
```batch
cd build
mingw32-make clean
mingw32-make -j4
```

### Debug Build:
```batch
qmake -r -spec win32-g++ CONFIG+=debug ..\LicenseChecker.pro
mingw32-make
```

---

## Production Deployment

### Create Distribution Folder:
```batch
REM Copy runtime files
mkdir dist
copy build\src\ui\LicenseCheckerUI.exe dist\
copy build\src\agent\LicenseCheckerAgent.exe dist\
copy C:\Qt\5.15.0\mingw81_64\bin\Qt5*.dll dist\
copy C:\Qt\5.15.0\mingw81_64\bin\libgcc_s_seh-1.dll dist\
copy C:\Qt\5.15.0\mingw81_64\bin\libstdc++-6.dll dist\
copy C:\Qt\5.15.0\mingw81_64\bin\libwinpthread-1.dll dist\
```

### Create Installer (Optional):
Use NSIS or WiX to package dist\ folder for end-users

---

## Next Steps

1. ✅ **Build:** Run `build-mingw.bat`
2. ✅ **Test:** Run `build\src\ui\LicenseCheckerUI.exe`
3. ✅ **Install:** Run service installer as admin
4. ✅ **Deploy:** Copy files to `C:\Program Files\LicenseChecker\`

For detailed documentation, see:
- [BUILD_MINGW.md](BUILD_MINGW.md) - MinGW/qmake guide
- [BUILD_GUIDE.md](BUILD_GUIDE.md) - CMake/MSVC guide
- [README.md](README.md) - Full documentation
