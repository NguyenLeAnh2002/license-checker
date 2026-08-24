# Implementation Plan: Windows License Detection

Spec: [docs/specs/license-detection/Specification.md](Specification.md)

## Approach

**Hybrid API Strategy (SL APIs + WMI + Registry)**

The agent will attempt license detection in a three-tier fallback sequence:
1. **Tier 1 (Primary):** Windows Software Licensing (SL) APIs via native C++ Windows headers — fastest and most accurate for Windows 10/11/Server
2. **Tier 2 (Fallback):** Windows Management Instrumentation (WMI) queries — covers older Windows 7/8 versions and handles systems where SL APIs are unavailable
3. **Tier 3 (Last Resort):** Direct registry inspection — validates findings and catches edge cases

**Why this approach:**
- Covers all Windows 7+ versions reliably
- Detects KMS activation across all tiers (KMS info available via all three methods)
- Gracefully degrades: if Tier 1 fails, falls back to Tier 2; if both fail, Tier 3 as last resort
- If all three fail, reports "Unable to Determine" (FR-6)

**Trade-offs considered:**
- Single API (Option A) would be simpler but fail on older Windows versions
- WMI-only (Option B) would be slower and less precise on modern systems
- Hybrid costs more implementation complexity, but gains robustness required by the spec

**Output Format:** JSON for both local notifications and server transmission (easy to parse, human-readable for logging/debugging)

**Runtime Model:** Runs as Windows Service with SYSTEM privileges (required for full API access). Detection runs in a background worker thread on a 5-minute interval, keeping the main agent thread responsive.

**State Management:** Failed detections are logged (not cached) — agent always attempts fresh detection on each cycle. Logging provides an audit trail for troubleshooting without adding persistent state complexity.

## File/Module Structure

| Path | Responsibility |
|------|-----------------|
| `src/license-detection/LicenseDetector.h` | Main entry point; orchestrates three-tier detection; returns structured result or error |
| `src/license-detection/SLAPIDetector.cpp/h` | Tier 1: Queries Windows SL APIs; detects license status and KMS server (Windows + Office) |
| `src/license-detection/WMIDetector.cpp/h` | Tier 2: WMI-based detection; fallback for older systems; queries license and KMS info |
| `src/license-detection/RegistryDetector.cpp/h` | Tier 3: Registry inspection; validates edge cases (grace periods, tampered installs) |
| `src/license-detection/LicenseResult.h` | Data structure: encapsulates detection output (status, timestamp, version, KMS server, error flags) |
| `src/license-detection/LicenseStatusEnum.h` | Enums: LicenseStatus (Legitimate, Cracked, NotLicensed, UnableToDetermine) and related constants |
| `src/license-detection/DetectionLogger.cpp/h` | Logs detection attempts (success/failure) for audit trail; no persistent state caching |
| `src/agent/LicenseDetectionWorker.cpp/h` | Background worker thread; runs 5-minute detection loop; reports results to notifications + server queue |
| `src/agent/ServiceMain.cpp` | Windows Service boilerplate; starts agent with SYSTEM privileges; registers worker thread |

## Testing Strategy

| Requirement | Verified By |
|---|---|
| **FR-1:** Detect via Windows APIs (not third-party) | Unit tests: mock SL API calls; verify detector correctly invokes native Windows APIs, not external tools |
| **FR-2:** Classify into Legitimate/Cracked/NotLicensed | Integration tests on test VMs: run detector against known license states (legitimate key, cracked key, unlicensed); verify classification |
| **FR-3:** Retrieve *currently active* license | Integration test: compare detector output against `slmgr.vbs /dli` output; verify it matches the running license, not historical |
| **FR-4/5:** Support Windows 7+ versions | Integration tests: run on Windows 7, 8.1, 10, 11, Server 2016/2019/2022; verify no crashes and correct status detection |
| **FR-6:** Report "Unable to Determine" on API failure | Unit test: mock SL/WMI/Registry to return errors; verify detector reports "Unable to Determine" gracefully without crashing |
| **FR-7:** Classify grace period as "NotLicensed" | Integration test: trigger grace period state on test VM; verify detector reports "NotLicensed" (not "Legitimate" or "Cracked") |
| **FR-8:** Continue reporting last-known status if detection fails | Integration test: simulate API failure on 2nd run; verify agent logs failure and continues using safe fallback; check logs for failure event |
| **FR-9:** Detect Windows KMS server address | Integration test on KMS-activated VM: verify detector retrieves and reports correct KMS server hostname/IP |
| **FR-10:** Detect Office KMS server address | Integration test: install Office on KMS; verify detector separately reports Office KMS server if different from Windows KMS |
| **FR-11:** Report "No KMS" if not KMS-activated | Integration test: run on non-KMS system; verify detector reports "No KMS" indicator (does not crash or fail) |
| **FR-12:** Include KMS info in output | Unit test: verify JSON output includes KMS fields; JSON schema validation test |
| **FR-13:** Output includes status + timestamp + version + KMS | Unit test: JSON parser; verify all required fields present and correctly typed |
| **FR-14:** Output available for notifications + server transmission | Unit test: verify LicenseResult object is serializable to JSON; mock server API endpoint receives correct JSON payload |
| **FR-15:** Detection completes quickly | Performance test: run detection 100 times; measure average/max completion time; assert < 10s (target: as fast as possible, likely 2-5s typical) |
| **FR-16:** 5-minute interval capability | Unit test: mock timer; verify worker thread schedules detection every 5 minutes (±skew tolerance) |

## File Structure Example

```
License-Checker/
├── src/
│   ├── license-detection/
│   │   ├── LicenseDetector.h
│   │   ├── LicenseDetector.cpp
│   │   ├── SLAPIDetector.h
│   │   ├── SLAPIDetector.cpp
│   │   ├── WMIDetector.h
│   │   ├── WMIDetector.cpp
│   │   ├── RegistryDetector.h
│   │   ├── RegistryDetector.cpp
│   │   ├── LicenseResult.h
│   │   ├── LicenseStatusEnum.h
│   │   ├── DetectionLogger.h
│   │   └── DetectionLogger.cpp
│   ├── agent/
│   │   ├── LicenseDetectionWorker.h
│   │   ├── LicenseDetectionWorker.cpp
│   │   ├── ServiceMain.cpp
│   │   └── ...
│   └── ...
├── tests/
│   ├── unit/
│   │   ├── LicenseDetectorTest.cpp
│   │   ├── SLAPIDetectorTest.cpp
│   │   ├── WMIDetectorTest.cpp
│   │   └── RegistryDetectorTest.cpp
│   ├── integration/
│   │   ├── LicenseDetectionIntegrationTest.cpp (runs on actual Windows systems)
│   │   └── KMSDetectionTest.cpp
│   └── ...
└── ...
```

## Risks / Open Questions

1. **[NEEDS CLARIFICATION]:** Should agent report activation method (KMS/Retail/OEM) in JSON output, or just KMS server address when applicable? This affects JSON schema design.
   - **Risk:** If not decided now, will create inconsistent behavior between Windows SL API output and fallback detectors
   - **Recommendation:** Resolve before implementation; affects all three detector tiers

2. **WMI Performance on Slow Systems:** WMI queries (Tier 2) can be slow on older machines or systems with heavy WMI load. Agent is running in background thread, so this shouldn't block the main agent, but detection cycles could exceed 5 minutes on slow systems.
   - **Risk:** If system is slow enough, next detection cycle starts before previous one completes
   - **Mitigation:** Implement task serialization (queue detection requests, run one at a time)

3. **Privilege Escalation:** Service runs as SYSTEM, which is the simplest approach but highest privilege. If agent has any vulnerabilities, they become SYSTEM-level vulnerabilities.
   - **Risk:** Supply chain or code defect escalates to SYSTEM compromise
   - **Mitigation:** Code review + minimal attack surface (license detection only, no script execution); consider if lower privileges could work (deferred to future optimization)

4. **Registry Inspection Edge Cases:** Direct registry access (Tier 3) can detect tampered license states, but Windows may also cache or obfuscate license data. Behavior varies by Windows version.
   - **Risk:** Registry-based detection may miss novel tampering methods or give false positives on some Windows versions
   - **Mitigation:** Extensive testing on all supported versions; Tier 1/2 should catch most cases; Tier 3 is best-effort

5. **Office Installation Detection:** If Office isn't installed, agent must report "Not Found" (FR-10/11). Currently proposed: check Office registry keys and report "Not Found" if absent.
   - **Risk:** What if Office is installed but in a broken state (incomplete install, corrupted registry)? Should this report "Not Found" or "Error"?
   - **Mitigation:** Define clear distinction in spec between "Not Found" (Office not installed) vs. "Error" (Office installed but detection failed)

6. **JSON Schema Versioning:** JSON output will be consumed by server API. If schema changes (e.g., new KMS fields), backward compatibility matters.
   - **Risk:** Old agents sending old JSON format to new server, or vice versa
   - **Mitigation:** Define JSON schema with version field; server accepts multiple versions during rollout

## Related ADRs

- No ADRs required for this plan (all decisions are within standard C++ service architecture; none are novel enough to warrant a record). If WMI vs SL performance becomes a problem, we can write an ADR when making a change.

## Notes for Implementation

- Start with SL API (Tier 1) implementation; it's the most direct and covers modern Windows
- Test early on actual Windows VMs, not mocks — license detection behavior varies subtly by version
- KMS detection is critical for the KMS Validation epic downstream; ensure this tier passes all KMS tests before marking complete
- JSON output should be validated against a schema; include schema definition in the repo
