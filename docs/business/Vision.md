# Vision: License Checker

## Status
Draft

## Problem
Organizations have no visibility into whether machines in their departments are running legitimate or cracked Windows and Office licenses. This creates compliance risk, prevents administrators from knowing the true state of their software inventory, and leaves security gaps from unauthorized software installations.

## Target Users / Market
IT administrators and compliance teams at mid-to-large organizations who need to monitor and manage software licensing across their user base and departments.

## Opportunity
By deploying lightweight agents on end-user machines, we can continuously monitor license authenticity and centralize reporting, enabling administrators to act on compliance issues and provide timely notifications to users.

## Goals
- G-1: Provide administrators real-time visibility into the license status (legitimate vs. cracked) of all Windows and Office installations across their organization, organized by department
- G-2: Alert end users on their machines when license status changes (e.g., license expires, detected as unlicensed)
- G-3: Enable administrators to generate reports on license compliance across departments for audit and management purposes
- G-4: Track license status changes over time so administrators can identify patterns and respond to compliance drift

## Success Metrics
- Administrators can identify 100% of machines with cracked licenses within their monitored departments within seconds
- License status changes (legitimate ↔ cracked) are detected and reported to both the agent and server within 5 minutes
- Administrators can generate a compliance report showing license status by department in under 1 minute
- End users receive notifications on their machines when license status changes

## Non-Goals
- Enforce or block unlicensed machines from operating (monitoring only, no enforcement)
- Automatically remediate or activate licenses
- Monitor software licenses beyond Windows and Office
- Replace official Microsoft licensing tools or compliance workflows
