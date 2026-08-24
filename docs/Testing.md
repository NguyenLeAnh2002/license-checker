# License Checker - Test Scenarios

## Test Environment Setup
- Windows 8/8.1/10/11 VM hoặc máy thực
- Agent: `LicenseCheckerAgent.exe` (service hoặc console mode)
- UI: `LicenseCheckerUI.exe`
- Log file: `license-detection.log` (cùng folder với Agent)

---

## Phase 1: Agent License Detection Tests

### Test-1.1: Detect Legitimate Windows License (WMI method)
**Precondition:** Windows được license hợp lệ, WMI SoftwareLicensingProduct có LicenseStatus=1

**Steps:**
1. Chạy Agent console mode: `LicenseCheckerAgent.exe`
2. Mở PowerShell chạy: 
   ```powershell
   Get-WmiObject -Namespace "root\cimv2" -Class SoftwareLicensingProduct | 
   Where-Object {$_.PartialProductKey -ne $null} | 
   Select-Object Name, LicenseStatus, PartialProductKey | Format-List
   ```
3. Xác nhận `LicenseStatus = 1`

**Expected Result:**
- Agent log: `[INFO] SUCCESS - License Status: Legitimate`
- `licenseStatus = 0` in JSON response
- UI display: "Status: **Licensed (Legitimate)**"

**Acceptance Criteria:**
- ✓ Log file có SUCCESS message
- ✓ UI show "Licensed (Legitimate)"
- ✓ Details show Name, Edition, PartialProductKey (không rỗng)

---

### Test-1.2: Detect Not Licensed Windows (WMI method)
**Precondition:** Windows chưa được license, WMI LicenseStatus=0 hoặc 2-8

**Steps:**
1. Chạy Agent
2. Verify WMI LicenseStatus value từ PowerShell
3. Xem agent log output

**Expected Result:**
- Agent log: `[INFO] SUCCESS - License Status: NotLicensed`
- `licenseStatus = 2` in JSON
- UI display: "Status: **Not Licensed**"

**Acceptance Criteria:**
- ✓ Log show NotLicensed
- ✓ UI show "Not Licensed"
- ✓ Windows Edition, Partial Product Key populated

---

### Test-1.3: Detect Cracked/Non-Genuine License (WMI method)
**Precondition:** Windows modified/tampered, WMI LicenseStatus > 8

**Steps:**
1. Chạy Agent
2. Verify WMI LicenseStatus > 8
3. Kiểm tra log

**Expected Result:**
- Agent log: `[INFO] SUCCESS - License Status: Cracked`
- `licenseStatus = 1` in JSON
- UI display: "Status: **Licensed (Cracked)**"

**Acceptance Criteria:**
- ✓ Detect as Cracked (not Legitimate)
- ✓ UI show warning indicator

---

### Test-1.4: Extract WMI Fields Correctly
**Precondition:** Any Windows license status

**Steps:**
1. Run Agent in console mode
2. Check log file for WMI extraction:
   ```
   [DEBUG]   WMI: Name = ...
   [DEBUG]   WMI: Description = ...
   [DEBUG]   WMI: PartialProductKey = ...
   [DEBUG]   WMI: ProductKeyChannel = ...
   [DEBUG]   WMI: ActivationId = ...
   ```
3. Send GetLicenseData command to pipe (via UI or manual test)
4. Verify JSON response contains all fields

**Expected Result:**
- JSON response has populated fields:
  ```json
  {
    "windows": {
      "name": "Windows(R), Education edition",
      "description": "Windows(R) Operating System, RETAIL channel",
      "partialProductKey": "7CFBY",
      "productKeyChannel": "Retail",
      "activationId": "e558417a-...",
      "applicationId": "55c92734-...",
      "extendedPid": "03612-03280-...",
      "installationId": "604240476...",
      "useLicenseUrl": "https://...",
      "validationUrl": "https://..."
    }
  }
  ```

**Acceptance Criteria:**
- ✓ All WMI fields extracted (not empty)
- ✓ Correct values match slmgr.vbs /dlv output
- ✓ JSON properly formatted

---

### Test-1.5: Handle Missing WMI Fields Gracefully
**Precondition:** WMI query returns some but not all fields

**Steps:**
1. Run Agent
2. Parse JSON response
3. Check which fields are populated vs empty

**Expected Result:**
- License status still detected correctly
- Missing fields default to "" (empty string)
- No crash or exception

**Acceptance Criteria:**
- ✓ Agent doesn't crash with partial data
- ✓ Returns status code even if some fields missing
- ✓ Log shows which fields were missing

---

### Test-1.6: Tier Fallback - Registry when WMI fails
**Precondition:** WMI query fails (WMI disabled or corrupt), Registry available

**Steps:**
1. Disable WMI or mock WMI failure
2. Run Agent
3. Check logs for fallback:
   ```
   [DEBUG]   Registry: Trying WMI SoftwareLicensingProduct query first...
   [DEBUG]   WMI query failed, falling back to Registry...
   ```

**Expected Result:**
- Agent still detects license status via Registry
- Correct status returned

**Acceptance Criteria:**
- ✓ Fallback to Registry tier working
- ✓ Status detected despite WMI failure
- ✓ Log shows fallback attempt

---

### Test-1.7: Windows Version & Edition Detection
**Precondition:** Any Windows system

**Steps:**
1. Run Agent
2. Check JSON response `version` and `edition` fields
3. Compare with system: `winver` command or Settings

**Expected Result:**
- Correct Windows version (8, 10, 11)
- Correct edition (Pro, Home, Education)
- JSON: `"version": 8, "edition": "Education"`

**Acceptance Criteria:**
- ✓ Version number correct
- ✓ Edition name correct
- ✓ Match system info

---

### Test-1.8: KMS Server Detection
**Precondition:** Windows configured with KMS OR no KMS

**Steps:**
1. Run Agent
2. Check `kmsStatus` and `kmsServer` fields
3. Verify via Registry: `HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\SoftwareProtectionPlatform\RPC`

**Expected Result (with KMS):**
- `"kmsStatus": 1` (KMSDetected)
- `"kmsServer": "kms.corp.local"` (actual server)

**Expected Result (without KMS):**
- `"kmsStatus": 0` (NotKMS)
- `"kmsServer": ""` (empty)

**Acceptance Criteria:**
- ✓ Detect KMS presence correctly
- ✓ Extract KMS server address if present
- ✓ Handle no-KMS case gracefully

---

## Phase 2: Named Pipe Communication Tests

### Test-2.1: Pipe Server Starts Successfully
**Precondition:** Agent running

**Steps:**
1. Start Agent service: `sc start LicenseCheckerAgent`
2. Check if pipe `\\.\pipe\LicenseChecker` is accessible
3. Verify with: `Get-ChildItem \\.\pipe\LicenseChecker` (PowerShell)

**Expected Result:**
- Pipe created and listening
- Accessible from other processes

**Acceptance Criteria:**
- ✓ Pipe exists and is accessible
- ✓ Service logs: "NamedPipeServer: Pipe created"

---

### Test-2.2: UI Connects to Pipe Successfully
**Precondition:** Agent running with pipe

**Steps:**
1. Start UI: `LicenseCheckerUI.exe`
2. Check `license-detection.log`:
   ```
   UI: Attempting to connect to pipe 'LicenseChecker'
   UI: Successfully connected to pipe 'LicenseChecker'
   ```

**Expected Result:**
- UI connects on first attempt or after retry
- Connection established within 5 seconds

**Acceptance Criteria:**
- ✓ Connection successful
- ✓ Pipe handle valid
- ✓ No timeout

---

### Test-2.3: UI Sends GetLicenseData Command
**Precondition:** UI connected to pipe

**Steps:**
1. UI sends "GetLicenseData" command
2. Check log:
   ```
   UI: Sending command - GetLicenseData
   Service: Received command from UI - GetLicenseData
   ```

**Expected Result:**
- Service receives command
- Responds with JSON data

**Acceptance Criteria:**
- ✓ Command transmitted successfully
- ✓ Service logs receipt
- ✓ Response sent back

---

### Test-2.4: Pipe Response Contains Valid JSON
**Precondition:** GetLicenseData sent

**Steps:**
1. Parse JSON response from pipe
2. Validate structure:
   ```json
   {
     "status": "success",
     "data": {
       "windows": { ... },
       "office": { ... }
     }
   }
   ```

**Expected Result:**
- Response is valid JSON
- Contains required fields
- No malformed data

**Acceptance Criteria:**
- ✓ JSON parseable
- ✓ All expected fields present
- ✓ No corrupt data in string fields

---

### Test-2.5: UI Sends RequestImmediateCheck Command
**Precondition:** UI connected to pipe

**Steps:**
1. Click "Check Now" button in UI
2. Check log for:
   ```
   UI: Sending command - RequestImmediateCheck
   Service: Received command from UI - RequestImmediateCheck
   Service: Immediate detection requested by UI
   ```
3. Wait 5 seconds for detection cycle to run

**Expected Result:**
- Service immediately triggers detection
- Detection cycle runs (instead of waiting 5 min)
- UI refreshes with latest data

**Acceptance Criteria:**
- ✓ Command received by service
- ✓ Detection cycle starts immediately
- ✓ UI shows updated status

---

### Test-2.6: Pipe Communication with Error Handling
**Precondition:** Service or pipe has issues

**Steps:**
1. Start UI with Agent stopped
2. Check log:
   ```
   UI: Attempting to connect to pipe 'LicenseChecker'
   UI: CreateFileA failed with error 2 (retry 1/5)
   ```
3. Start Agent while UI is running
4. UI should auto-reconnect

**Expected Result:**
- UI retries connection multiple times
- Reconnects when service becomes available
- Graceful error messages

**Acceptance Criteria:**
- ✓ Retry logic working
- ✓ Auto-reconnect on recovery
- ✓ No crash on communication failure

---

## Phase 3: UI Display Tests

### Test-3.1: Display License Status Correctly
**Precondition:** UI connected to Agent

**Steps:**
1. Open UI
2. Check main "License Status" display
3. Verify value matches actual Windows license status

**Expected Result (Licensed):**
- Display: "Status: **Licensed (Legitimate)**"
- Green/positive indicator

**Expected Result (Not Licensed):**
- Display: "Status: **Not Licensed**"
- Red/warning indicator

**Expected Result (Cracked):**
- Display: "Status: **Licensed (Cracked)**"
- Red/error indicator

**Acceptance Criteria:**
- ✓ Status string correct
- ✓ Visual indicator appropriate
- ✓ Update on refresh

---

### Test-3.2: Display Windows Details Tab
**Precondition:** UI showing data

**Steps:**
1. Click Windows License tab
2. Verify displayed fields:
   - Name
   - Edition
   - Description
   - License Status Detail
   - Product Key (partial)
   - KMS Status
   - KMS Server
   - Last Detected timestamp

**Expected Result:**
- All fields populated with data from Agent
- Format clear and readable
- Timestamps in ISO format

**Acceptance Criteria:**
- ✓ All fields displayed
- ✓ Data matches JSON response
- ✓ No truncation/corruption

---

### Test-3.3: Display Office License Tab (if applicable)
**Precondition:** UI showing data

**Steps:**
1. Click Office License tab
2. Check for populated fields or "Not Detected"

**Expected Result:**
- Show Office status if installed and licensed
- Show "Not Detected" if Office not found
- Display correct status

**Acceptance Criteria:**
- ✓ Office detection working or gracefully showing N/A
- ✓ Tab displays without crashing

---

### Test-3.4: Display Settings Tab
**Precondition:** UI running

**Steps:**
1. Click Settings tab
2. Verify displayed information:
   - Hostname
   - Machine GUID
   - Department (if configured)
   - Service Status
   - Last Report time

**Expected Result:**
- System information displayed
- Service status shows "Running" or "Stopped"
- Timestamps accurate

**Acceptance Criteria:**
- ✓ System info correct
- ✓ Service status accurate
- ✓ No sensitive info exposed

---

### Test-3.5: Auto-Refresh Data (5-minute timer)
**Precondition:** UI running

**Steps:**
1. Note current timestamp
2. Wait 5+ minutes
3. Check if UI refreshes automatically
4. Verify timestamp updated in log

**Expected Result:**
- UI queries Agent every 5 minutes
- Status/data updated
- No user action required

**Acceptance Criteria:**
- ✓ Auto-refresh timer working
- ✓ Data updates every 5 min
- ✓ Log shows refresh attempts

---

### Test-3.6: Manual Refresh via Check Now Button
**Precondition:** UI running

**Steps:**
1. Click "Check Now" button
2. Check log for immediate detection
3. Verify UI updates with latest data

**Expected Result:**
- Detection runs immediately (not 5 min wait)
- Data refreshed on UI
- Button responsive

**Acceptance Criteria:**
- ✓ Immediate detection triggered
- ✓ UI updates within 2 seconds
- ✓ Button press registered

---

## Phase 4: Service Installation & Lifecycle Tests

### Test-4.1: Service Installs Successfully
**Precondition:** LicenseCheckerAgent.exe built and available

**Steps:**
1. Run: `sc create LicenseCheckerAgent binPath= "C:\path\to\LicenseCheckerAgent.exe" start= auto`
2. Verify: `sc query LicenseCheckerAgent`

**Expected Result:**
- Service created with status "STOPPED"
- Display name correct
- Start type is "AUTO_START"

**Acceptance Criteria:**
- ✓ Service registered in Service Control Manager
- ✓ `sc query` shows correct status
- ✓ Service properties correct

---

### Test-4.2: Service Starts Successfully
**Precondition:** Service installed

**Steps:**
1. Run: `sc start LicenseCheckerAgent`
2. Check: `sc query LicenseCheckerAgent`
3. Verify pipe created and listening

**Expected Result:**
- Service state: "RUNNING"
- No error in Event Viewer
- Pipe \\.\pipe\LicenseChecker created
- Log file created

**Acceptance Criteria:**
- ✓ Service status is RUNNING
- ✓ No startup errors in logs
- ✓ Pipe accessible
- ✓ Log file present

---

### Test-4.3: Service Stops Gracefully
**Precondition:** Service running

**Steps:**
1. Run: `sc stop LicenseCheckerAgent`
2. Check: `sc query LicenseCheckerAgent`
3. Verify pipe closes

**Expected Result:**
- Service state: "STOPPED"
- Pipe no longer accessible
- Graceful shutdown in log

**Acceptance Criteria:**
- ✓ Service stops within 5 seconds
- ✓ No errors in Event Viewer
- ✓ Pipe closes cleanly

---

### Test-4.4: Service Auto-Starts on Boot
**Precondition:** Service installed with auto start

**Steps:**
1. Restart machine
2. After boot, run: `sc query LicenseCheckerAgent`
3. Verify service running

**Expected Result:**
- Service automatically started
- Status "RUNNING" after boot
- No manual intervention needed

**Acceptance Criteria:**
- ✓ Service auto-starts on boot
- ✓ Pipe available immediately
- ✓ No errors in Event Viewer

---

### Test-4.5: Console Mode (Testing without Service)
**Precondition:** LicenseCheckerAgent.exe available

**Steps:**
1. Run: `LicenseCheckerAgent.exe` (no /install)
2. Check output/log for detection
3. Test pipe availability

**Expected Result:**
- Agent runs in console mode
- Outputs detection results to log
- Pipe still available for UI
- No service required

**Acceptance Criteria:**
- ✓ Console mode working
- ✓ License detected and logged
- ✓ Pipe accessible to UI
- ✓ Can be stopped with Ctrl+C

---

## Phase 5: Edge Cases & Error Handling

### Test-5.1: Handle WMI Timeout
**Precondition:** WMI query takes too long

**Steps:**
1. Simulate slow WMI response
2. Run Agent
3. Check if detection completes or times out

**Expected Result:**
- Agent doesn't hang indefinitely
- Fallback to Registry if WMI slow
- Returns status within 10 seconds

**Acceptance Criteria:**
- ✓ No hang/freeze
- ✓ Status returned
- ✓ Log shows fallback attempt

---

### Test-5.2: Handle Missing Registry Values
**Precondition:** Some registry values missing/corrupted

**Steps:**
1. Run Agent
2. Check logs for partial data handling
3. Verify status still detected

**Expected Result:**
- Status detected despite missing Registry values
- Missing fields default to empty string
- No crash

**Acceptance Criteria:**
- ✓ Graceful handling of missing data
- ✓ No exception/crash
- ✓ Status still returned

---

### Test-5.3: Handle Pipe Client Disconnect
**Precondition:** UI connected to Agent

**Steps:**
1. UI connected and receiving data
2. Kill Agent process: `taskkill /IM LicenseCheckerAgent.exe /F`
3. Watch UI behavior

**Expected Result:**
- UI detects disconnection in log
- Shows "Connection lost"
- Attempts to reconnect when Agent restarted

**Acceptance Criteria:**
- ✓ Disconnection detected
- ✓ Graceful error message (no crash)
- ✓ Auto-reconnect on Agent restart

---

### Test-5.4: Handle Concurrent Pipe Connections
**Precondition:** Multiple UI instances

**Steps:**
1. Start Agent
2. Start multiple UI instances
3. Each should connect to same pipe
4. All should receive data

**Expected Result:**
- All UI instances connect successfully
- Pipe server handles multiple clients
- Data consistent across all UIs

**Acceptance Criteria:**
- ✓ Multiple connections work
- ✓ No data corruption
- ✓ All clients updated

---

### Test-5.5: Handle Rapid Refresh Requests
**Precondition:** UI connected

**Steps:**
1. Click "Check Now" multiple times rapidly
2. Observe detection cycle behavior

**Expected Result:**
- Detection queued/debounced
- Only one cycle runs at a time
- No race conditions

**Acceptance Criteria:**
- ✓ No concurrent detection cycles
- ✓ Queue handled properly
- ✓ Last data returned is latest

---

## Phase 6: Logging & Diagnostics Tests

### Test-6.1: Log File Contains All Debug Information
**Precondition:** Agent running, detection complete

**Steps:**
1. Check `license-detection.log` file
2. Verify contains:
   - Timestamps for each action
   - DEBUG messages from detection
   - SUCCESS/ERROR markers
   - WMI field extraction details

**Expected Result:**
- Log is comprehensive
- Can trace entire detection flow
- Useful for troubleshooting

**Acceptance Criteria:**
- ✓ All major events logged
- ✓ Timestamps accurate
- ✓ Log is readable and organized

---

### Test-6.2: Log Rotation/Management
**Precondition:** Agent running for extended period

**Steps:**
1. Run Agent for several detection cycles (30+ min)
2. Check log file size
3. Verify no log explosion

**Expected Result:**
- Log grows at reasonable rate
- No unbounded growth
- Old entries managed

**Acceptance Criteria:**
- ✓ Log file manageable size
- ✓ No performance impact
- ✓ Can review full history

---

## Test Execution Tracking

### Test Results Template
```
| Test ID | Status | Notes | Timestamp |
|---------|--------|-------|-----------|
| Test-1.1 | ✓ PASS | | 2026-08-07 |
| Test-1.2 | ✓ PASS | | 2026-08-07 |
| Test-2.1 | ✓ PASS | | 2026-08-07 |
| Test-3.1 | ✓ PASS | | 2026-08-07 |
```

---

## Test Automation (Future)
- [ ] Automated detection tests via PowerShell scripts
- [ ] Pipe communication tests via C# harness
- [ ] Service lifecycle tests via batch scripts
- [ ] Log validation via regex patterns
- [ ] Performance/load testing with concurrent connections

---

## Known Issues & Workarounds

### Issue-1: WMI Query Returns Empty Fields
**Symptom:** `licenseStatusDetail`, `name`, `description` showing empty in response

**Workaround:** 
- Ensure code extracts all WMI fields correctly
- Fields must be populated in `WindowsLicenseInfo` struct
- Call `result.SetWindowsLicenseInfo(info)` to save to response

**Resolution:** See Test-1.4, implement field extraction in `QueryWMISoftwareLicensingProduct()`

---

### Issue-2: Notification License Status (0xC004F034)
**Symptom:** slmgr shows "Notification" status but WMI LicenseStatus=2

**Workaround:**
- System shows as "Not Licensed" (correct behavior)
- Notification means license needs attention/renewal
- Status correctly mapped from WMI value

**Resolution:** No action needed - detection working as designed

---

## Sign-off
- [ ] All Phase 1 tests passed
- [ ] All Phase 2 tests passed
- [ ] All Phase 3 tests passed
- [ ] All Phase 4 tests passed
- [ ] All Phase 5 tests passed
- [ ] All Phase 6 tests passed
- [ ] Ready for production deployment

**Tested by:** _______________
**Date:** _______________
**Notes:** _______________
