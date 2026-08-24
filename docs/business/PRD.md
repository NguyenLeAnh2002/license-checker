# PRD: License Checker

Vision: [docs/business/Vision.md](Vision.md)

## Status
Draft

## Summary
License Checker is a centralized compliance monitoring system that continuously detects Windows and Office license authenticity across an organization, delivers real-time visibility to administrators organized by department, and alerts end users when their license status changes. The system enables IT teams to maintain compliance without enforcing or blocking machines — monitoring and reporting only.

## Epics

### Epic-1: License Detection Engine
- **Priority:** Must
- **Scope:** The C++ agent component that runs on user machines and detects whether Windows and Office licenses are legitimate or cracked. This is the core sensing capability — without it, the entire system has no data.
- **Future spec slug:** `license-detection`

### Epic-2: Agent Installation & Deployment
- **Priority:** Must
- **Scope:** Standalone installer and setup flow for deploying the C++ agent to user machines. Includes first-run configuration, machine registration with the server, and dependency installation. This enables adoption at scale.
- **Future spec slug:** `agent-installer`

### Epic-3: Server & License Data Hub
- **Priority:** Must
- **Scope:** FastAPI backend that receives license status reports from agents, stores license data and machine metadata, manages user accounts and department hierarchies, and exposes data to the admin dashboard. This is the central data authority and coordination point.
- **Future spec slug:** `server-api`

### Epic-4: Admin Dashboard
- **Priority:** Must
- **Scope:** Web-based interface for administrators to view all monitored machines, their license status (legitimate/cracked), and organized view by department. Includes real-time or near-real-time status updates.
- **Future spec slug:** `admin-dashboard`

### Epic-5: Client-Side Notifications
- **Priority:** Must
- **Scope:** End users receive pop-up notifications on their machines when license status changes (e.g., transitions from legitimate to cracked, or expires). Keeps users informed without requiring them to check a dashboard.
- **Future spec slug:** `client-notifications`

### Epic-6: User & Department Management
- **Priority:** Must
- **Scope:** Administrators can create and manage departments, create/manage user accounts, and organize machines by department. This underpins the organizational structure and enables scoped visibility and reporting.
- **Future spec slug:** `user-department-management`

### Epic-7: Compliance Report Generation
- **Priority:** Should
- **Scope:** Administrators can generate compliance reports showing license status by department and user, with export to PDF or CSV. Supports audit and management needs.
- **Future spec slug:** `compliance-reporting`

### Epic-8: License Status History & Trend Analysis
- **Priority:** Could
- **Scope:** Maintain historical records of license status changes over time. Admins can view trends (e.g., rising number of cracked licenses in a department) to identify patterns and drift.
- **Future spec slug:** `license-history`

## Out of Scope (product-level)
- Integration with third-party IT management tools (Active Directory, MDM, SIEM)
- Enforcement or remediation (blocking machines, auto-activation)
- Support for software licenses beyond Windows and Office
- Mobile app (web-based admin interface only)

## Constraints
- C++ agent must be lightweight and have minimal impact on machine performance
- Agent must work across multiple Windows versions (Windows 10, Windows 11, Windows Server)
- Server API should support high-volume concurrent license status reports from many agents
- All inter-system communication must use secure, encrypted channels
