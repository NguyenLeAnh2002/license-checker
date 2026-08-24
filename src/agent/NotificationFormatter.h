#pragma once

#include "../license-detection/LicenseResult.h"
#include <string>

// Formats license detection results as human-readable notification messages
// Designed for non-technical end users (office-worker persona)
class NotificationFormatter {
public:
    // Format a LicenseResult into a notification message (FR-14)
    static std::string FormatNotification(const LicenseResult& result);

private:
    // Helper: Format Windows license status message
    static std::string FormatWindowsStatus(const LicenseResult& result);

    // Helper: Format advisory message for compliance issues
    static std::string FormatAdvisory(LicenseStatus status);

    // Helper: Get user-friendly status label
    static std::string GetStatusLabel(LicenseStatus status);
};
