# Test License Checker Agent Service

## Prerequisites

- Windows 7 SP1 or newer
- Qt5.15 LTS with MinGW 8.1
- Administrator privileges (for service operations)

## Quick Build

### Batch Script (Easiest)
```batch
set Qt5_DIR=C:\Qt\5.15.0\mingw81_64
build-agent-only.bat
```

### PowerShell
```powershell
$env:Qt5_DIR = "C:\Qt\5.15.0\mingw81_64"
.\build-agent-only.ps1
```

### Manual
```batch
set Qt5_DIR=C:\Qt\5.15.0\mingw81_64
set PATH=%Qt5_DIR%\bin;%PATH%

mkdir build-agent
cd build-agent
qmake -spec win32-g++ CONFIG+=release ..\AgentOnly.pro
mingw32-make -j4
cd ..
```

## Output
```
build-agent\release\LicenseCheckerAgent.exe  (~2-3 MB)
```

---

## Test Modes

### 1. Test Mode (No Installation)
**Use this to verify it works before installing as service:**

```batch
build-agent\release\LicenseCheckerAgent.exe /test
```

**Expected output:**
```
License Checker Agent - Test Mode
Starting license detection...
[License detection results]
Test completed successfully
```

---

### 2. Install as Windows Service

**Run as Administrator:**

```batch
REM Install
build-agent\release\LicenseCheckerAgent.exe /install

REM Start service
net start LicenseCheckerAgent

REM Check status
sc query LicenseCheckerAgent
```

**Expected status:**
```
SERVICE_NAME: LicenseCheckerAgent
        TYPE               : 10  WIN32_OWN_PROCESS
        STATE              : 4  RUNNING
        WIN32_EXIT_CODE    : 0  (0x0)
        SERVICE_EXIT_CODE  : 0  (0x0)
        CHECKPOINT         : 0x0
        WAIT_HINT          : 0x0
```

---

### 3. Manual Test

**What the Agent Does:**
1. ✅ Detects Windows license (using SL API / WMI / Registry)
2. ✅ Creates Named Pipe `\\.\pipe\LicenseChecker`
3. ✅ Listens for UI connections
4. ✅ Runs detection every 5 minutes
5. ✅ Writes license data to registry

**Test Commands:**

```batch
REM Start in debug mode
build-agent\release\LicenseCheckerAgent.exe /debug

REM Check registry
reg query HKLM\SOFTWARE\LicenseCheckerAgent

REM Check event viewer
eventvwr.msc
  → Windows Logs → Application → Look for "LicenseChecker" entries

REM Check service status
sc query LicenseCheckerAgent

REM View service config
sc qc LicenseCheckerAgent
```

---

## Test Steps

### Step 1: Build
```batch
set Qt5_DIR=C:\Qt\5.15.0\mingw81_64
build-agent-only.bat
```

### Step 2: Run Test Mode
```batch
build-agent\release\LicenseCheckerAgent.exe /test
```

If successful, you should see:
- License status detected
- No errors
- Program exits cleanly

### Step 3: Install Service
```batch
REM Run as Administrator
build-agent\release\LicenseCheckerAgent.exe /install
net start LicenseCheckerAgent
```

### Step 4: Verify Running
```batch
sc query LicenseCheckerAgent
```

Look for: `STATE : 4 RUNNING`

### Step 5: Check License Data
```batch
REM View registry
reg query HKLM\SOFTWARE\LicenseCheckerAgent

REM Check event logs
eventvwr.msc
```

### Step 6: Test Named Pipe Communication
Run UI in another terminal:
```batch
build\release\LicenseCheckerUI.exe
```

UI should connect and show license data.

### Step 7: Stop Service
```batch
net stop LicenseCheckerAgent
sc query LicenseCheckerAgent
```

Should show: `STATE : 1 STOPPED`

### Step 8: Uninstall Service
```batch
build-agent\release\LicenseCheckerAgent.exe /uninstall
```

---

## Troubleshooting

### Error: "Access Denied"
- Run Command Prompt as Administrator
- Or use PowerShell with `-Install` flag

### Error: "Service already exists"
- Uninstall first: `AgentOnly.exe /uninstall`
- Wait 2 seconds
- Install again

### Service won't start
- Check Event Viewer for error messages
- Verify registry permissions
- Run test mode first: `/test`

### No license detected
- Ensure running as Administrator
- Check Windows Event Viewer
- Verify WMI service is running: `Get-Service -Name WinRM`

### Named Pipe connection fails
- Service must be running: `sc query LicenseCheckerAgent`
- Check pipe exists: `netstat -an | findstr pipe`
- Restart service: `net stop LicenseCheckerAgent` → `net start LicenseCheckerAgent`

---

## Log Files

### Service Logs
**Location:** `%APPDATA%\LicenseCheckerAgent\license-detection.log`

**View:**
```batch
type "%APPDATA%\LicenseCheckerAgent\license-detection.log"
```

### Event Viewer Logs
```batch
eventvwr.msc
→ Windows Logs
→ Application
→ Search for "LicenseChecker"
```

### Registry Data
```batch
reg query HKLM\SOFTWARE\LicenseCheckerAgent
```

---

## Test Checklist

- [ ] Build successful (no errors)
- [ ] Test mode runs: `/test` shows license status
- [ ] Service installs: `/install` succeeds
- [ ] Service starts: `net start LicenseCheckerAgent` succeeds
- [ ] Service running: `sc query LicenseCheckerAgent` shows RUNNING
- [ ] Registry populated: `reg query HKLM\SOFTWARE\LicenseCheckerAgent`
- [ ] Event logs show activity
- [ ] Service stops: `net stop LicenseCheckerAgent` succeeds
- [ ] Service uninstalls: `/uninstall` succeeds

---

## Performance

**Resource Usage (at idle):**
- Memory: ~5-10 MB
- CPU: <1% (between cycles)
- Disk: Minimal (logs only)

**Detection Time:**
- First detection: 2-5 seconds
- Periodic detection: 3-10 seconds
- Total cycle time: ~5 minutes

---

## Next Steps

After successful Agent test:

1. **Build UI**
   ```batch
   build-ui-only.bat
   build\release\LicenseCheckerUI.exe
   ```

2. **Test UI + Agent Communication**
   - Keep Agent running
   - Start UI
   - Check if license data appears

3. **Full System Test**
   - Both Agent and UI running
   - Click "Check Now" in UI
   - Verify license status updates

---

## Support

For issues:
1. Check Event Viewer (eventvwr.msc)
2. Review log file: `%APPDATA%\LicenseCheckerAgent\license-detection.log`
3. Run test mode: `LicenseCheckerAgent.exe /test`
4. Check registry: `reg query HKLM\SOFTWARE\LicenseCheckerAgent`
