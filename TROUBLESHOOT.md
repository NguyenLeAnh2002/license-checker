# Build & Run Troubleshooting Guide

> ⚠️ This replaces the old version of this doc, which covered qmake/MinGW/Qt5
> errors and a separate Windows-service agent. None of that applies anymore
> - the current build is MSBuild-only (no Qt) and the app is a single exe
> with no service to install. See [BUILD_VS2022.md](BUILD_VS2022.md) for
> the current build path.

## Error: "MSBuild : error : Cannot find msbuild.exe" / `msbuild` not recognized

### Cause
Visual Studio / Build Tools isn't installed, or its MSBuild isn't in PATH.

### Solution
```batch
where msbuild
```
If not found, either open a "Developer Command Prompt for VS" (which sets
PATH automatically), or point at it directly:
```batch
set PATH=%PATH%;C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin
build-msbuild.bat
```

---

## Error: "The build tools for v142 (Platform Toolset = 'v142') cannot be found"

### Cause
Both `.vcxproj` files target `PlatformToolset v142` (the VS2019 C++ toolset),
which is an optional component separate from Visual Studio itself.

### Solution
Open the **Visual Studio Installer** → Modify → **Individual components**
tab → check **"MSVC v142 - VS 2019 C++ x64/x86 build tools"** → Modify.

(If you'd rather not install v142, retargeting both `.vcxproj` files to
`v143` also works - the code has no v142-specific dependency. But v142 is
what's currently installed on both machines this project builds on, so
that's the supported/tested path.)

---

## Error: "Cannot find project file" / "LicenseChecker.sln not found"

### Cause
Not running from the repository root.

### Solution
```batch
cd D:\path\to\license-checker
build-msbuild.bat
```

---

## Error: "unresolved external symbol ..." linking `LicenseCheckerUI`

### Cause
`LicenseDetection` (the static library `LicenseCheckerUI` depends on)
wasn't built, or you built `LicenseCheckerUI.vcxproj` in isolation instead
of the whole solution.

### Solution
Build the solution, not a single project, so the dependency order in
`LicenseChecker.sln` is respected:
```batch
MSBuild LicenseChecker.sln /p:Configuration=Release /p:Platform=x64 /t:Rebuild
```

---

## Build is slow

```batch
MSBuild LicenseChecker.sln /p:Configuration=Release /p:Platform=x64 /m
```
`/m` builds projects in parallel (there are only two here, so the gain is
modest). Use Release rather than Debug for anything you intend to actually
run - Debug is meaningfully slower to build and to run.

---

## Executable Won't Run

### "This app can't run on your PC" / wrong architecture
Make sure you copied the file from `x64\Release\`, not `x64\Debug\` mixed
with a different platform, and that the target machine is 64-bit (all
supported Windows 7 SP1+ targets in practice are).

### It runs but does nothing / no window
Check `license_checker_startup.log` next to the exe for a `FATAL:` line -
`main.cpp` catches and logs unhandled exceptions there before the process
exits. Also check Windows Event Viewer → Application logs.

There are **no DLLs to copy** - `LicenseCheckerUI.exe` only links against
standard Windows system libraries (`winhttp.lib`, `crypt32.lib`, etc.) that
ship with Windows itself, so "DLL not found" errors point at something
else (a genuinely corrupted copy, or an antivirus quarantine - check Event
Viewer/Windows Defender history).

---

## License Detection Shows "Unknown" / Missing Data

### Cause
Not running elevated, or the relevant Windows subsystem isn't available.

### Solution
- Run as Administrator (SL API and some WMI queries need it for full results)
- Check `license-detection.log` next to the exe for `LogFailure` entries
  from the specific detector (SL API / WMI / Registry / slmgr / ospp)
- Verify WMI is running: `Get-Service -Name Winmgmt`

---

## Not Reporting to the Controller

### Cause
No server address is configured for this exe.

### Solution
Check `license-detection.log` for `ServerReporter:` lines - they say
explicitly whether an embedded address was found, whether
`agent_config.json` was found, and whether the POST succeeded. See
[README.md](README.md)'s Troubleshooting section, `AGENT_SERVER_PROTOCOL_REAL.md`,
and `license_checker_server/internal/checkerstamp` in the sibling repo for
how the embedded address gets stamped in.

---

## Still Having Issues?

**Full build output:**
```batch
MSBuild LicenseChecker.sln /p:Configuration=Release /p:Platform=x64 /v:detailed
```

**When asking for help, include:**
1. Full error message/output
2. `msbuild -version` output
3. Whether "MSVC v142 - VS 2019 C++ x64/x86 build tools" is installed
   (Visual Studio Installer → Modify → Individual components)
4. `license_checker_startup.log` and `license-detection.log`, if the issue
   is at runtime rather than build time
