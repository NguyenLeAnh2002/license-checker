# Persona: IT Manager

## Summary
An IT administrator or compliance manager responsible for overseeing software licensing compliance across their department or division. They actively monitor license status, manage organizational structures, and use compliance data to make decisions and report to leadership.

## Goals
- Maintain real-time visibility into the license status (legitimate or cracked) of all machines in their department(s)
- Quickly identify which machines are at compliance risk
- Access reliable compliance data for reporting and audits
- Organize machines and users by department for scoped management

## Pain Points
- Currently have no visibility into which machines in their department have legitimate vs. cracked licenses
- Unable to proactively identify and address compliance risks; may discover issues only during audits
- Time-consuming to manually check compliance status across machines
- No centralized way to track which departments are most affected by unlicensed software

## Context of Use
- **Frequency:** Daily — actively checks the dashboard to monitor license status
- **Device:** Both mobile and desktop (responsive web interface needed for flexibility)
- **Environment:** At their desk for detailed reporting; also mobile checks for quick status updates while in meetings or traveling
- **Time pressure:** High — need to act quickly when compliance issues are identified

## Technical Proficiency
**Medium** — Comfortable navigating admin dashboards, reports, and system settings. Can:
- Use filters, search, and organizational views (by department, by status)
- Generate and export reports (PDF, CSV)
- Manage user accounts and department structures
- Understand compliance terminology and license states

**UX implication:** Dashboard can assume familiarity with IT concepts (e.g., "cracked license," "compliance status") but should provide intuitive navigation and clear data visualization without requiring technical troubleshooting knowledge.

## Relationship to Vision/PRD
- **Vision Goal G-1:** Provide administrators real-time visibility into license status organized by department
- **Vision Goal G-3:** Enable administrators to generate compliance reports
- **Vision Goal G-4:** Track license status changes over time
- **Epic-4:** Admin Dashboard — this persona's primary interface
- **Epic-6:** User & Department Management — enables organizational structure this persona needs
- **Epic-7:** Compliance Report Generation — critical capability for audit and reporting needs
