# Testing License Checker

> ⚠️ This replaces the old version of this doc, which tested a separate
> `LicenseCheckerAgent.exe` Windows service communicating with the UI over
> a named pipe (`/install`, `/test`, `sc query`, `HKLM\SOFTWARE\...`). None
> of that exists anymore - there is no service and no named pipe. Detection
> and UI live in one process, `LicenseCheckerUI.exe`, which you just run.

## Prerequisites

- Windows 7 SP1 or newer
- Administrator privileges recommended (SL API/WMI detection is more
  complete when elevated)

## Build

```batch
build-msbuild.bat
```
See [BUILD_VS2022.md](BUILD_VS2022.md) for prerequisites (no Qt needed).

## Running It

```batch
x64\Release\LicenseCheckerUI.exe
```

Just run the exe - there's nothing to install, no service to start. It
opens its window, runs an initial detection cycle immediately, then
repeats every 5 minutes for as long as the window stays open. Click
**"Check Now"** to trigger a cycle on demand.

**Expected on first run:**
- Windows License tab populates with status, product key (partial), KMS info
- Office License tab populates the same way (if Office is installed)
- Settings tab shows hostname/machine GUID/department and the last report time

## Log Files

Both log files appear **next to the exe**, not in `%APPDATA%` or the
registry:

```
x64\Release\license_checker_startup.log   - process start / fatal crash log
x64\Release\license-detection.log         - detection results + ServerReporter activity
```

```batch
type x64\Release\license-detection.log
```

`ServerReporter:` lines in that file tell you whether it found a
controller address (embedded trailer or `agent_config.json`) and whether
the report POST succeeded - see [README.md](README.md)'s Troubleshooting
section and `AGENT_SERVER_PROTOCOL_REAL.md`.

## Manual Test Steps

1. **Build**, as above.
2. **Run** `x64\Release\LicenseCheckerUI.exe` - a window should appear
   within a couple seconds with all three tabs populated.
3. **Check the logs** - `license-detection.log` should show a completed
   detection cycle with no `LogFailure` entries for the detectors
   themselves.
4. **Click "Check Now"** - the tabs should refresh and a new detection
   entry should appear in the log.
5. **If a controller is configured** (embedded address or
   `agent_config.json`), confirm `license-detection.log` shows
   `ServerReporter: report sent successfully to ...` and that the report
   shows up in the controller's UI.
6. **Close the window** - the process should exit cleanly (check Task
   Manager, no orphaned process).

## Troubleshooting

### No license detected / fields show "Unknown"
- Run as Administrator (required for full SL API and WMI access)
- Check `license-detection.log` for `LogFailure` entries from the individual detectors
- Verify WMI service is running: `Get-Service -Name Winmgmt`

### Window doesn't appear / crashes on launch
- Check `license_checker_startup.log` for a `FATAL:` line
- Check Windows Event Viewer → Application logs

### Not reporting to a controller
- `license-detection.log` will say explicitly whether it found an embedded
  address, an `agent_config.json`, or neither - see README.md's
  Troubleshooting section for what each case means

## Performance

**Resource usage (idle, window open):**
- Memory: ~5-10 MB
- CPU: <1% between cycles

**Detection time:** a few seconds per cycle; repeats every 5 minutes.
