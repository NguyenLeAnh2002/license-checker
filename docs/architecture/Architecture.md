# Architecture: License Checker

PRD: [docs/business/PRD.md](../business/PRD.md)

Last updated: 2026-08-06

## Overview

License Checker is a distributed monitoring system with three tiers: lightweight C++ agents deployed on end-user machines that detect license status and display local notifications, a centralized FastAPI server running on-premises that receives and stores license data, and a web-based admin dashboard where IT managers view compliance status organized by department, manage users/departments, and generate reports. The agent operates independently with periodic license checks; the server is the single source of truth for compliance data; the dashboard provides administrative visibility and reporting.

## Components

| Component | Responsibility | Serves Epics |
|---|---|---|
| **C++ License Detection Agent** | Runs on user machines. Periodically detects Windows and Office license authenticity (legitimate vs. cracked). Displays popup notifications after each check showing current status. Sends status data to server. Handles Windows 10, 11, and Server variants. | Epic-1 (License Detection), Epic-2 (Agent Installer), Epic-5 (Client Notifications) |
| **Agent Installer & Setup** | Standalone installer for deploying the C++ agent to user machines. Handles installation, initial configuration, server endpoint registration, and dependency setup. Generates unique machine identifier for tracking. | Epic-2 (Agent Installer) |
| **FastAPI Server** | Central backend running on-premises. Receives license status reports from agents via HTTP API. Stores license data, machine metadata, user accounts, and department hierarchies in a local database. Exposes REST API for the admin dashboard. Handles authentication for admin users. | Epic-3 (Server & Data Hub), Epic-6 (User & Department Management) |
| **Local Database** | SQLite or equivalent running on the server machine. Stores: license status snapshots, machine records (ID, owner, department, last check time), user accounts (admin credentials), department hierarchies, and historical license status for trend analysis. | Epic-3 (Server & Data Hub), Epic-4 (Admin Dashboard), Epic-8 (History & Trends) |
| **Admin Dashboard (Web)** | Browser-based interface for IT managers. Displays real-time or near-real-time license status of all machines organized by department. Supports filtering, search, and drill-down by user/department. Enables report generation (PDF/CSV export). Supports user and department management. Responsive design for desktop and mobile. | Epic-4 (Admin Dashboard), Epic-6 (User & Department Management), Epic-7 (Compliance Reporting) |

## Key Data Flows

1. **License Check & Notification (Agent → User)**
   - Agent wakes on schedule (configurable interval)
   - Detects current Windows and Office license status using OS APIs
   - Immediately displays popup notification to user with status ("Windows: Legitimate | Office: Cracked")
   - Continues to background task of sending status to server

2. **License Data Upload (Agent → Server)**
   - Agent sends HTTP POST to server with: machine ID, Windows license status, Office license status, timestamp, machine metadata
   - Includes API key or simple auth token for identification (mockup level)
   - Server stores in database; returns 200 OK

3. **Admin Dashboard Query (Dashboard → Server)**
   - Admin logs in with username/password (database-stored)
   - Dashboard queries server for all machines in their department(s)
   - Server returns: machine list with latest license status, owner, last-check time
   - Dashboard refreshes on user action (filter, search, manual refresh)

4. **Report Generation (Dashboard → Server → Export)**
   - Admin selects date range and department
   - Server queries database for compliance data
   - Dashboard formats and exports PDF/CSV
   - User downloads locally

## Cross-Cutting Decisions

- **Authentication (Admin):** Username/password stored in server database. No external directory integration (AD/LDAP) in initial version. Sessions managed via HTTP cookies or simple JWT token (mockup level).
  
- **Agent Authentication:** Simple API key or token sent with each request (mockup; will be enhanced in later phases for production security).

- **Communication:** HTTP (not HTTPS initially for mockup); agents use standard HTTP POST to `/api/license-status` endpoint. To be upgraded to HTTPS with certificate pinning in production.

- **Data Storage:** Single SQLite database on the server machine. Simple schema: machines table, license_checks table (historical), users table, departments table. Backup strategy: manual file backup (mockup; production will need automated backups).

- **Deployment:** Server runs as a service/process on a dedicated local machine accessible to agent machines over network. Agents deployed via installer MSI or equivalent; can be pushed via Group Policy or manual installation.

- **Notification Timing:** Notifications triggered immediately after local license check completes — not reactive to status changes. End users see status each time the agent runs its periodic check.

## Known Constraints / Technical Debt

- **Mockup-Level Security:** Initial implementation uses simple authentication without encryption. Production version will require HTTPS, certificate validation, and stronger auth mechanisms.

- **Database Scalability:** SQLite on same server works for departmental-scale deployments (hundreds of machines). Enterprise-scale deployments (thousands of agents) may require migration to PostgreSQL or equivalent. Current architecture not optimized for high-concurrency writes.

- **Agent Update Mechanism:** No built-in auto-update for agents; manual reinstallation required for updates. Future version should add delta updates or auto-upgrade capability.

- **Notification Delivery:** Notifications are purely local to the agent machine; no server-side alert escalation or email notifications to admins about critical compliance issues (could be added in future epic).

- **Offline Agents:** If an agent cannot reach the server, it continues to run and display local notifications but cannot report data. No retry/queue mechanism; will be added in production.

## Architecture Decisions to Revisit

As features are built and requirements evolve, anticipate potential ADRs around:
- Migration from SQLite to a separate database service (performance/scalability)
- Upgrade from HTTP to HTTPS + certificate management
- Agent auto-update/delta deployment strategy
- Historical data retention and archival policy
- Dashboard caching strategy for large deployments
