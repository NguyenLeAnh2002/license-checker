# Specification: Windows License Detection

## Status
Clarified

## Overview
The License Detection Engine must detect the current license status of Windows on the local machine where the agent is running. This is the foundational capability for the License Checker system — without reliable detection, the admin dashboard and notifications have no data. The agent will query Windows licensing APIs to determine whether Windows is legitimately licensed, cracked, or not licensed at all. Additionally, if Windows or Office is activated via KMS (Key Management Service), the agent must detect and report the KMS server address; validation of that KMS server against an authorized list is handled server-side (separate KMS Validation epic).

## User Scenarios
- As an end user (office-worker persona), I need the agent to detect my Windows license status so that I understand whether my machine is compliant.
- As an IT manager (it-manager persona), I need reliable license detection data so that I can trust the compliance status shown in the dashboard.

## Functional Requirements

### Detection & Classification
- **FR-1:** The system MUST detect the current Windows license status by querying native Windows licensing APIs (not third-party tools or heuristics).
- **FR-2:** The system MUST classify the detected Windows license into one of three distinct states:
  - **Legitimate:** Windows is properly licensed and activated
  - **Cracked:** Windows is using an unauthorized/pirated license or has been tampered with
  - **Not Licensed:** Windows is installed but has never been licensed/activated
- **FR-3:** The system MUST retrieve the currently *active* license state — the license that Windows is currently running under, not historical or alternative licenses.

### Windows Version Support
- **FR-4:** The system MUST support license detection on Windows 7 and all later Windows versions, including:
  - Windows 7 (Home, Professional, Enterprise)
  - Windows 8 / 8.1 (Core, Pro, Enterprise)
  - Windows 10 (Home, Pro, Enterprise, Education)
  - Windows 11 (Home, Pro, Enterprise, Education)
- **FR-5:** The system MUST support license detection on Windows Server versions 2008 and later, including:
  - Windows Server 2008 / 2008 R2
  - Windows Server 2012 / 2012 R2
  - Windows Server 2016
  - Windows Server 2019
  - Windows Server 2022

### Error Handling & Robustness
- **FR-6:** If the system cannot query Windows licensing APIs (due to permissions, API unavailability, or system errors), it MUST report the status as **"Unable to Determine"** rather than guessing or reporting a default value.
- **FR-7:** The system MUST handle the case where Windows is in a grace period (post-unactivated state, pre-revocation) and classify it as **"Not Licensed"**.
- **FR-8:** The system MUST continue to report the last-known license status if a detection attempt fails, rather than going silent. The agent should flag the status as stale or unverified.

### KMS Detection (if applicable)
- **FR-9:** If Windows is activated via KMS, the agent MUST detect and report the KMS server address.
- **FR-10:** If Office is activated via KMS, the agent MUST detect and report the Office KMS server address (separately from Windows KMS, if different).
- **FR-11:** KMS server detection MUST be attempted for both Windows and Office; if a KMS server cannot be detected or determined (e.g., not KMS-activated), the agent MUST report as "No KMS" or similar indicator rather than failing.
- **FR-12:** KMS server information (address/hostname) MUST be included in the output sent to the server. [NEEDS CLARIFICATION: Should agent also report KMS status (e.g., "Activated via KMS," "Activated via Retail," "Activated via OEM") or only the server address if KMS?]

### Output Format
- **FR-13:** The system MUST output license detection results in a structured format that includes: detected license status, timestamp of detection, Windows version/edition detected, and (if applicable) KMS server address(es).
- **FR-14:** The system MUST make this output available to the local system (for notifications) and prepare it for transmission to the server (in a format to be defined by the server-api spec).

### Performance & Frequency
- **FR-15:** License detection MUST complete as quickly as possible to minimize agent overhead on the user's machine.
- **FR-16:** The agent MUST be capable of running detection on a 5-minute interval by default (configurable per deployment if needed).

## Out of Scope
- **Office license detection** — covered by a separate epic (office-detection)
- **KMS server validation** — agent detects and reports KMS server address; server-side validation against authorized KMS list is a separate KMS Validation epic
- **Remediation or enforcement** — this spec detects status only; taking action on detected status (e.g., blocking, notifying Microsoft) is out of scope
- **Third-party software licenses** — only Windows; no other OS or application licenses
- **Historical license tracking** — this spec covers point-in-time detection; historical trend analysis is in the License History epic
- **External license verification** — detection uses local OS APIs only; no external Microsoft server checks
- **Detailed license metadata** (e.g., license key, activation ID) beyond KMS server address — only the classification (Legitimate/Cracked/Not Licensed) and KMS info if present

## Open Questions
- **[NEEDS CLARIFICATION]:** When the agent detects KMS activation, should it also report the activation method (e.g., "Activated via KMS," "Activated via Retail," "Activated via OEM") in addition to the KMS server address, or just the server address?

## Acceptance Criteria
- [ ] Agent successfully detects Windows license status on Windows 10, 11, and Server 2016/2019/2022
- [ ] License status is correctly classified as Legitimate, Cracked, Not Licensed, or Unable to Determine
- [ ] Agent retrieves the *currently active* license, not historical alternatives
- [ ] Agent detects if Windows is activated via KMS and reports the KMS server address
- [ ] Agent detects if Office is activated via KMS and reports the Office KMS server address
- [ ] If no KMS is detected, agent reports "No KMS" or equivalent indicator (does not fail)
- [ ] Agent handles API failures gracefully without crashing or guessing; reports "Unable to Determine" if detection fails
- [ ] Detected status (timestamp + state + KMS info if applicable) is available for local notifications and server transmission
- [ ] Detection completes as quickly as possible without blocking other agent tasks
- [ ] Agent runs detection on a 5-minute interval by default
- [ ] No crashes or unhandled exceptions when running on supported Windows versions
