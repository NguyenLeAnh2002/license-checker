# Deploying License Checker to Another Machine

> ⚠️ This replaces the old version of this guide, which described copying
> Qt5 DLLs and `src/ui`/`src/common` source files that no longer exist in
> this repository. The current app is a single statically-simple native
> Win32 exe with **no runtime DLL dependencies of its own** beyond standard
> Windows system DLLs every Windows install already has - there is nothing
> to bundle alongside it.

## The short version

1. Build once (see [BUILD_VS2022.md](BUILD_VS2022.md)):
   ```batch
   build-msbuild.bat
   ```
2. Copy `x64\Release\LicenseCheckerUI.exe` to the target machine. That's it -
   no DLLs, no Qt runtime, no installer.
3. Run it directly - no install/service step.

`create-package.bat`/`create-package.ps1` in this repo still reference the
old `src/ui`/`src/common` layout and predate this single-exe app - don't
use them; a plain file copy of the one exe is the whole deployment.

## Real-world deployment path: via the controller

In practice this exe isn't normally hand-copied - it's uploaded once to
`license_checker_server` (the controller), which embeds its own address
into the exe and serves it for download to every machine on the network.
See that repo's `internal/checkerstamp` package and
`AGENT_SERVER_PROTOCOL_REAL.md` in this repo for the wire protocol.

If the exe also needs to be Authenticode-signed (so it isn't flagged as an
unsigned/unknown binary), the build/release order is:

```
build (MSBuild) → sign (signtool) → upload to controller
```

No manual packaging step in between - the controller's upload handler
appends the address-stamping trailer itself (`internal/checkerstamp.EnsureTrailer`
in that repo) right after receiving the upload, in a way that survives
re-stamping without invalidating the signature. `scripts/AppendTrailer.ps1`
in that repo has the same logic in PowerShell, kept only for offline/manual
testing - it's not part of the normal admin workflow. If a raw, unsigned
exe gets uploaded, the controller can still stamp it (there's no signature
to preserve), it just won't be Authenticode-signed.

## Manual configuration fallback

If an exe wasn't stamped with a controller address (or you want to
override one), drop an `agent_config.json` next to `LicenseCheckerUI.exe`:
```json
{ "server_url": "http://192.168.1.10:8080", "api_key": "" }
```
See `ServerReporter::LoadConfig()` in `src/ui-native/ServerReporter.cpp`
for the exact precedence rules (embedded address vs. this file).

## Verification Checklist

- [ ] `x64\Release\LicenseCheckerUI.exe` runs on the source machine
- [ ] Copied exe runs on the target machine without installing anything
- [ ] `license-detection.log` / `license_checker_startup.log` appear next
      to the exe after running it once
- [ ] `license-detection.log` shows either an embedded server address or an
      `agent_config.json` being picked up (see Troubleshooting in
      [README.md](README.md))
- [ ] Windows 7 SP1 or newer

## File Size Reference

| Item | Size |
|------|------|
| `LicenseCheckerUI.exe` (Release) | ~500 KB |
| Everything needed to run it | just that one file |

## Network Deployment

Since it's a single small file with no dependencies, any transport works
fine - network share, USB, cloud drive, or (the real path) download from
the controller's `/download` endpoint.
