# Build License Checker Agent with Visual Studio 2022

## Quick Start

### Option 1: Using Build Script (Easiest)
```batch
build-msbuild.bat
```

### Option 2: Using Visual Studio GUI
```
1. Open LicenseCheckerAgent.sln in Visual Studio 2022
2. Set configuration: Release | x64
3. Build → Build Solution (Ctrl+Shift+B)
4. Output: x64\Release\LicenseCheckerAgent.exe
```

### Option 3: Using Command Line
```batch
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" ^
  LicenseCheckerAgent.sln /p:Configuration=Release /p:Platform=x64
```

---

## Step-by-Step GUI Build

### Step 1: Install Visual Studio 2022
If not already installed:
- Download: https://visualstudio.microsoft.com/downloads/
- Select "C++ Desktop Development" workload
- Install

### Step 2: Open Solution
```
File → Open → Project/Solution
Select: LicenseCheckerAgent.sln
```

### Step 3: Select Configuration
- Configuration: **Release** (dropdown top-left)
- Platform: **x64**

### Step 4: Build
```
Build → Build Solution (Ctrl+Shift+B)
```

Wait for build to complete. Should see:
```
========== Build: 2 succeeded, 0 failed ==========
```

### Step 5: Output
```
x64\Release\LicenseCheckerAgent.exe  (~500 KB)
```

---

## Build Output Structure

```
LicenseChecker\
├── LicenseCheckerAgent.sln
├── x64\
│   ├── Release\
│   │   ├── LicenseCheckerAgent.exe      ← Final executable
│   │   ├── LicenseDetection.lib         ← Static library
│   │   └── *.obj                        ← Object files
│   └── Debug\
│       └── ...
├── src\
│   ├── license-detection\
│   │   └── LicenseDetection.vcxproj
│   └── agent\
│       └── LicenseCheckerAgent.vcxproj
```

---

## Test the Built Executable

### Test Mode (No Installation)
```batch
x64\Release\LicenseCheckerAgent.exe /test
```

Expected: Shows license detection results

### Install as Windows Service
Run as Administrator:
```batch
x64\Release\LicenseCheckerAgent.exe /install
net start LicenseCheckerAgent
sc query LicenseCheckerAgent
```

Expected: Service starts and shows RUNNING state

### Uninstall
```batch
net stop LicenseCheckerAgent
x64\Release\LicenseCheckerAgent.exe /uninstall
```

---

## Project Structure

### LicenseDetection.vcxproj (Static Library)
- **Type:** Static Library (.lib)
- **Output:** `x64\Release\LicenseDetection.lib`
- **Files:**
  - LicenseResult.cpp/h
  - LicenseDetector.cpp/h
  - SLAPIDetector.cpp/h
  - WMIDetector.cpp/h
  - RegistryDetector.cpp/h
  - DetectionLogger.cpp/h

### LicenseCheckerAgent.vcxproj (Executable)
- **Type:** Console Application (.exe)
- **Output:** `x64\Release\LicenseCheckerAgent.exe`
- **Dependencies:** LicenseDetection.lib
- **Files:**
  - ServiceMain.cpp
  - LicenseDetectionWorker.cpp/h
  - ServiceInstaller.cpp
  - NotificationFormatter.cpp/h
  - NamedPipeServer.cpp/h
  - SharedLicenseDataWriter.cpp/h

### Solution Configuration
- **Platforms:** x64 (64-bit Windows)
- **Configurations:** Debug, Release
- **Toolset:** v143 (Visual Studio 2022)
- **C++ Standard:** C++17
- **Character Set:** Unicode

---

## Dependencies & Libraries

### System Libraries (Linked Automatically)
- `advapi32.lib` - Windows Service API
- `kernel32.lib` - Windows Kernel
- `ws2_32.lib` - Windows Sockets
- `wbemuuid.lib` - WMI (Windows Management Instrumentation)
- `ole32.lib` - COM Object Linking & Embedding
- `oleaut32.lib` - OLE Automation
- `crypt32.lib` - Cryptography API
- `wtsapi32.lib` - Windows Terminal Services API

### Included Source
- License detection logic (custom)
- Named Pipe IPC (custom)
- Windows Service wrapper (custom)

**No external dependencies - fully self-contained!**

---

## Build Configuration Details

### Release Build
- Optimization: Maximum Speed
- Runtime: Multi-threaded DLL (MSVCRT)
- Debug Info: None (optimized for production)
- Size: ~500 KB

### Debug Build
- Optimization: Disabled
- Runtime: Multi-threaded Debug DLL
- Debug Info: Full (PDB file)
- Size: ~2-3 MB (with debug symbols)

---

## Troubleshooting

### Error: "Cannot find msbuild.exe"
Visual Studio 2022 not installed or not in PATH

**Solution:**
```batch
set PATH=%PATH%;C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin
build-msbuild.bat
```

### Error: "Cannot find project file"
Make sure running from repository root directory

**Solution:**
```batch
cd C:\License-checker
build-msbuild.bat
```

### Error: "Missing include files"
vcxproj include paths incorrect

**Solution:**
- Verify folder structure matches repository
- Check AdditionalIncludeDirectories in .vcxproj

### Error: "Unresolved external symbols"
Library linking failed

**Solution:**
1. Ensure LicenseDetection builds first (it's a dependency)
2. Check Windows SDK installed: `Control Panel → Programs → Windows SDK`
3. Rebuild all: `Build → Clean Solution`, then rebuild

### Build slow or hanging
Visual Studio indexing in progress

**Solution:**
- Wait for indexing to complete
- Or disable IntelliSense: `Tools → Options → Text Editor → C/C++ → Advanced → Disable IntelliSense`

---

## Performance Tips

### Faster Builds
1. Use Release configuration for final build
2. Enable parallel compilation:
   - `Tools → Options → Projects and Solutions → VC++ Project Settings`
   - Set "Maximum Parallel Project Builds" to your CPU cores

### Smaller Executable
Release build is already optimized (~500 KB)

### Faster Incremental Builds
Only changed files recompile automatically

---

## Command Line Builds

### Clean Build
```batch
"%MSBUILD_PATH%" LicenseCheckerAgent.sln /p:Configuration=Release /p:Platform=x64 /t:Clean
```

### Build Only
```batch
"%MSBUILD_PATH%" LicenseCheckerAgent.sln /p:Configuration=Release /p:Platform=x64 /t:Build
```

### Rebuild
```batch
"%MSBUILD_PATH%" LicenseCheckerAgent.sln /p:Configuration=Release /p:Platform=x64 /t:Rebuild
```

### Verbose Output
```batch
"%MSBUILD_PATH%" LicenseCheckerAgent.sln /p:Configuration=Release /p:Platform=x64 /v:detailed
```

---

## Next Steps

1. ✅ Build Agent: `build-msbuild.bat`
2. ✅ Test: `x64\Release\LicenseCheckerAgent.exe /test`
3. ✅ Install: `x64\Release\LicenseCheckerAgent.exe /install`
4. ✅ Verify: `sc query LicenseCheckerAgent`
5. Build UI: `build-ui-only.bat` (requires Qt5)
6. Test UI + Agent communication

---

## Support

For issues:
1. Check Visual Studio Build Output window
2. Review error messages carefully
3. Verify all source files exist
4. Ensure Visual Studio 2022 is up-to-date
