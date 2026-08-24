# Tasks: Windows License Detection

Plan: [docs/specs/license-detection/ImplementationPlan.md](ImplementationPlan.md)

---

## Task-1 — Define License Status Enums and Constants
- [x] Status: Complete
- Depends on: none
- Goal: Create foundational data structures for license status classification
- Files touched:
  - `src/license-detection/LicenseStatusEnum.h`
- Definition of done:
  - Header file defines `enum class LicenseStatus { Legitimate, Cracked, NotLicensed, UnableToDetermine }`
  - Enums for KMS status: `enum class KMSStatus { NotKMS, KMSDetected, KMSNotFound, Error }`
  - Constants defined for Windows versions (Windows7, Windows8, Windows10, Windows11, ServerX)
  - Enum-to-string conversion functions defined
  - Unit test passes: verify each enum value maps correctly to string representation

---

## Task-2 — Implement LicenseResult Data Structure
- [x] Status: Complete
- Depends on: Task-1
- Goal: Create the output data structure that holds detection results
- Files touched:
  - `src/license-detection/LicenseResult.h`
  - `src/license-detection/LicenseResult.cpp`
- Definition of done:
  - LicenseResult class encapsulates: license status, timestamp, Windows version/edition, KMS server address (Windows + Office), error flag, detection timestamp
  - Getters/setters for all fields
  - Constructor and copy/move semantics
  - Unit test passes: verify LicenseResult can be constructed, modified, and all fields retrieved correctly (FR-13)

---

## Task-3 — Implement DetectionLogger Infrastructure
- [x] Status: Complete
- Depends on: none
- Goal: Set up logging for detection attempts (success/failure audit trail)
- Files touched:
  - `src/license-detection/DetectionLogger.h`
  - `src/license-detection/DetectionLogger.cpp`
- Definition of done:
  - DetectionLogger class with methods: LogSuccess(LicenseResult), LogFailure(error_message), LogError(exception)
  - Writes logs to Windows Event Log or local file (configurable)
  - Thread-safe logging (handles concurrent writes from background worker)
  - Unit test passes: verify logs are written and can be read back (FR-8)

---

## Task-4 — Implement SLAPIDetector (Tier 1: Windows SL APIs)
- [x] Status: Complete
- Depends on: Task-1, Task-2
- Goal: Implement the primary license detection method using Windows SL APIs
- Files touched:
  - `src/license-detection/SLAPIDetector.h`
  - `src/license-detection/SLAPIDetector.cpp`
- Definition of done:
  - SLAPIDetector class with method: `LicenseResult Detect()`
  - Queries Windows SL APIs (slui.exe context, licensing API headers)
  - Detects and classifies Windows license status (Legitimate/Cracked/NotLicensed)
  - Detects Windows KMS server address if applicable
  - Detects Office (all versions) license status via same APIs
  - Detects Office KMS server address separately from Windows KMS
  - Reports "No KMS" if not KMS-activated
  - Handles errors gracefully (API unavailable, permission denied) → reports "UnableToDetermine"
  - Unit test passes: mock Windows SL APIs, verify correct license classification (FR-1, FR-2, FR-3, FR-9, FR-10, FR-11)
  - Performance test passes: detection completes in <10 seconds on test system (FR-15)

---

## Task-5 — Implement WMIDetector (Tier 2: WMI Fallback)
- [x] Status: Complete
- Depends on: Task-1, Task-2
- Goal: Implement the secondary detection method using WMI for older Windows versions
- Files touched:
  - `src/license-detection/WMIDetector.h`
  - `src/license-detection/WMIDetector.cpp`
- Definition of done:
  - WMIDetector class with method: `LicenseResult Detect()`
  - Uses WMI queries to detect Windows license status
  - Supports Windows 7, 8, 8.1 (versions where SL APIs may be unavailable)
  - Detects Windows KMS server address via WMI if applicable
  - Detects Office license status via WMI queries
  - Detects Office KMS server address separately
  - Reports "No KMS" if not KMS-activated
  - Handles WMI errors gracefully (WMI unavailable, query failures) → reports "UnableToDetermine"
  - Unit test passes: mock WMI queries, verify detection on older Windows versions (FR-4, FR-5, FR-9, FR-10, FR-11)
  - Performance test passes: detection completes in <10 seconds (FR-15)

---

## Task-6 — Implement RegistryDetector (Tier 3: Registry Fallback)
- [x] Status: Complete
- Depends on: Task-1, Task-2
- Goal: Implement the tertiary detection method using Windows registry inspection
- Files touched:
  - `src/license-detection/RegistryDetector.h`
  - `src/license-detection/RegistryDetector.cpp`
- Definition of done:
  - RegistryDetector class with method: `LicenseResult Detect()`
  - Queries Windows registry for license state information (HKEY_LOCAL_MACHINE\SYSTEM\..., license keys)
  - Detects Windows license status via registry inspection
  - Detects KMS server address from registry if applicable
  - Detects Office license status via registry
  - Detects Office KMS server address from registry
  - Reports "No KMS" if not KMS-activated
  - Handles registry access errors gracefully (permission denied, key not found) → reports "UnableToDetermine"
  - Identifies grace period state and classifies as "NotLicensed" (FR-7)
  - Unit test passes: mock registry access, verify detection logic (FR-4, FR-5, FR-7, FR-9, FR-10, FR-11)
  - Performance test passes: detection completes in <10 seconds (FR-15)

---

## Task-7 — Implement LicenseDetector Orchestrator
- [x] Status: Complete
- Depends on: Task-4, Task-5, Task-6
- Goal: Implement the three-tier fallback logic that tries SL API → WMI → Registry
- Files touched:
  - `src/license-detection/LicenseDetector.h`
  - `src/license-detection/LicenseDetector.cpp`
- Definition of done:
  - LicenseDetector class with method: `LicenseResult Detect()`
  - Attempts SL API detection (Tier 1) first
  - If Tier 1 succeeds, returns result immediately
  - If Tier 1 fails, attempts WMI detection (Tier 2)
  - If Tier 2 succeeds, returns result
  - If Tier 2 fails, attempts Registry detection (Tier 3)
  - If Tier 3 succeeds, returns result
  - If all three fail, returns "UnableToDetermine" status (FR-6)
  - Unit test passes: mock all three detectors, verify fallback logic works correctly
  - Integration test passes: run orchestrator on Windows VM, verify it calls tiers in correct order
  - Test on multiple Windows versions (7, 8.1, 10, 11, Server 2016/2019/2022) (FR-4, FR-5)

---

## Task-8 — Implement JSON Serialization for LicenseResult
- [x] Status: Complete
- Depends on: Task-2, Task-7
- Goal: Convert LicenseResult to JSON format for server transmission and notifications
- Files touched:
  - `src/license-detection/LicenseResult.h` (add serialization methods)
  - `src/license-detection/LicenseResult.cpp` (implement serialization)
  - `src/license-detection/LicenseResultSchema.json` (JSON schema definition)
- Definition of done:
  - LicenseResult::ToJSON() method returns JSON string representation
  - JSON includes all required fields: status, timestamp, Windows version, Windows KMS server, Office KMS server
  - JSON schema file documents the structure and field types
  - Unit test passes: verify LicenseResult serializes correctly to valid JSON (FR-12, FR-13, FR-14)
  - JSON schema validation test passes: verify output matches schema
  - Parse test passes: JSON can be deserialized back to equivalent LicenseResult object

---

## Task-9 — Implement LicenseDetectionWorker (Background Thread)
- [x] Status: Complete
- Depends on: Task-7, Task-3
- Goal: Create the background worker that runs detection on a 5-minute interval
- Files touched:
  - `src/agent/LicenseDetectionWorker.h`
  - `src/agent/LicenseDetectionWorker.cpp`
- Definition of done:
  - LicenseDetectionWorker class wraps LicenseDetector
  - Runs detection in a background worker thread
  - Scheduled to run every 5 minutes (configurable)
  - Uses task serialization: if previous detection is still running, queues next request (doesn't start concurrent detections)
  - After each detection, logs result (success or failure) via DetectionLogger
  - Provides methods: Start(), Stop(), GetLastResult()
  - Unit test passes: mock timer, verify worker runs detection every 5 minutes (FR-16)
  - Unit test passes: verify task serialization (if detection takes >5 min, next run waits for current to finish)
  - Unit test passes: verify logging is called after detection

---

## Task-10 — Implement Windows Service Integration (ServiceMain)
- [x] Status: Complete
- Depends on: Task-9
- Goal: Integrate agent with Windows Service Control Manager
- Files touched:
  - `src/agent/ServiceMain.cpp`
  - `src/agent/ServiceInstaller.cpp` (if needed for installation)
- Definition of done:
  - ServiceMain implements Windows Service entry point (ServiceMain, HandlerFunction)
  - Starts LicenseDetectionWorker on service start
  - Stops worker gracefully on service stop
  - Runs with SYSTEM privileges (required for full API access)
  - Service can be installed/uninstalled via Windows SC command
  - Manual test passes: install service, verify it starts automatically, verify detection runs every 5 minutes
  - Manual test passes: stop service gracefully, verify no orphaned threads or resources

---

## Task-11 — Implement Notification Output Interface
- [x] Status: Complete
- Depends on: Task-8
- Goal: Format detection results for end-user popup notifications
- Files touched:
  - `src/agent/NotificationFormatter.h`
  - `src/agent/NotificationFormatter.cpp`
- Definition of done:
  - NotificationFormatter class converts LicenseResult to human-readable notification message
  - Message format: "Windows: [Legitimate|Cracked|Not Licensed|Unable to Determine]" + "Office: [status]"
  - For cracked/unlicensed licenses, includes advisory message
  - Message is concise and non-technical (suitable for office-worker persona)
  - Unit test passes: verify notification messages for all status combinations
  - Message matches the JSON output (both describe same license state) (FR-14)

---

## Task-12 — Unit Tests: SLAPIDetector Error Cases
- [x] Status: Complete
- Depends on: Task-4
- Goal: Verify error handling in SL API tier
- Files touched:
  - `tests/unit/SLAPIDetectorTest.cpp`
- Definition of done:
  - Test: SL API unavailable → returns "UnableToDetermine"
  - Test: Permission denied → returns "UnableToDetermine"
  - Test: Malformed API response → handles gracefully, returns "UnableToDetermine"
  - All tests pass (FR-6)

---

## Task-13 — Unit Tests: WMIDetector Error Cases
- [x] Status: Complete
- Depends on: Task-5
- Goal: Verify error handling in WMI tier
- Files touched:
  - `tests/unit/WMIDetectorTest.cpp`
- Definition of done:
  - Test: WMI unavailable → returns "UnableToDetermine"
  - Test: Query timeout → returns "UnableToDetermine"
  - Test: Invalid query result → handles gracefully, returns "UnableToDetermine"
  - All tests pass (FR-6)

---

## Task-14 — Unit Tests: RegistryDetector Edge Cases
- [x] Status: Complete
- Depends on: Task-6
- Goal: Verify edge case handling in registry tier
- Files touched:
  - `tests/unit/RegistryDetectorTest.cpp`
- Definition of done:
  - Test: Grace period detection → classifies as "NotLicensed"
  - Test: Registry key missing → returns "UnableToDetermine"
  - Test: Permission denied on registry access → returns "UnableToDetermine"
  - Test: Corrupted registry data → handles gracefully
  - All tests pass (FR-6, FR-7)

---

## Task-15 — Integration Test: License Detection on Windows 10
- [x] Status: Complete
- Depends on: Task-7
- Goal: Verify detection works correctly on Windows 10
- Files touched:
  - `tests/integration/LicenseDetectionIntegrationTest.cpp`
- Definition of done:
  - Manual test on Windows 10 VM with legitimately licensed Windows: detector returns "Legitimate"
  - Manual test on Windows 10 VM with unlicensed Windows: detector returns "NotLicensed"
  - Compare detector output vs. `slmgr.vbs /dli` to ensure accuracy (FR-3)
  - All tests pass (FR-2, FR-4)

---

## Task-16 — Integration Test: License Detection on Older Windows (7, 8.1)
- [x] Status: Complete
- Depends on: Task-7
- Goal: Verify detection works on older Windows versions
- Files touched:
  - `tests/integration/LicenseDetectionIntegrationTest.cpp`
- Definition of done:
  - Manual test on Windows 7 VM: detector correctly identifies license status
  - Manual test on Windows 8.1 VM: detector correctly identifies license status
  - Verify fallback to WMI/Registry tiers if SL API unavailable
  - All tests pass (FR-4, FR-5)

---

## Task-17 — Integration Test: KMS Detection (Windows)
- [x] Status: Complete
- Depends on: Task-7
- Goal: Verify Windows KMS detection works correctly
- Files touched:
  - `tests/integration/KMSDetectionTest.cpp`
- Definition of done:
  - Manual test on KMS-activated Windows machine: detector reports KMS server address correctly
  - Verify KMS server hostname/IP is extracted accurately
  - Test non-KMS machine: detector reports "No KMS" (does not crash)
  - All tests pass (FR-9, FR-11)

---

## Task-18 — Integration Test: KMS Detection (Office)
- [x] Status: Complete
- Depends on: Task-7
- Goal: Verify Office KMS detection works separately from Windows
- Files touched:
  - `tests/integration/KMSDetectionTest.cpp`
- Definition of done:
  - Manual test: Install Office 2019 on KMS-activated machine, detector reports Office KMS server address separately
  - Manual test: Install Microsoft 365 on KMS-activated machine, detector reports Office KMS server
  - Verify Office KMS differs from Windows KMS (if applicable)
  - Test machine without Office: detector reports "Not Found" for Office (does not crash)
  - All tests pass (FR-10, FR-11)

---

## Task-19 — Integration Test: Grace Period Handling
- [x] Status: Complete
- Depends on: Task-7
- Goal: Verify grace period is correctly classified as "NotLicensed"
- Files touched:
  - `tests/integration/LicenseDetectionIntegrationTest.cpp`
- Definition of done:
  - Manual test: Trigger grace period on test VM (unactivate Windows, enter grace period)
  - Detector reports "NotLicensed" (not "Legitimate" or "Cracked")
  - Test passes (FR-7)

---

## Task-20 — Integration Test: Full Service Deployment
- [x] Status: Complete
- Depends on: Task-10, Task-9
- Goal: Verify Windows Service runs detection continuously
- Files touched:
  - `src/agent/ServiceMain.cpp`
  - `tests/integration/ServiceIntegrationTest.cpp`
- Definition of done:
  - Install service on test machine
  - Verify service starts automatically
  - Verify detection runs every 5 minutes (monitor logs)
  - Verify results are available to notifications
  - Stop service and verify graceful shutdown (no orphaned threads)
  - All acceptance criteria met (FR-16)

---

## Task-21 — Performance Test: Detection Completion Time
- [x] Status: Complete
- Depends on: Task-7
- Goal: Verify detection completes quickly
- Files touched:
  - `tests/performance/DetectionPerformanceTest.cpp`
- Definition of done:
  - Run detection 100 times on test system
  - Measure average completion time
  - Measure max completion time
  - Verify average < 5 seconds, max < 10 seconds
  - Test passes (FR-15)

---

## Task-22 — End-to-End Test: Full Detection Workflow
- [x] Status: Complete
- Depends on: Task-20, Task-11, Task-8
- Goal: Verify complete workflow from detection to notification to server transmission
- Files touched:
  - `tests/integration/EndToEndTest.cpp`
- Definition of done:
  - Service runs detection
  - Result is serialized to JSON
  - Notification message is formatted and ready for display
  - JSON is ready for transmission to server
  - Manual verification: detection output is correct, notification is appropriate, JSON is valid
  - All acceptance criteria met (FR-1 through FR-16)

