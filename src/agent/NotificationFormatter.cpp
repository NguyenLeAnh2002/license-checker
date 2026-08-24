#include "NotificationFormatter.h"
#include <sstream>

// Format a LicenseResult into a notification message
std::string NotificationFormatter::FormatNotification(const LicenseResult& result) {
    std::ostringstream message;

    // Header
    message << "License Status Report\n";
    message << "=====================\n\n";

    // Windows license status
    message << FormatWindowsStatus(result) << "\n";

    // Advisory message if needed
    std::string advisory = FormatAdvisory(result.GetLicenseStatus());
    if (!advisory.empty()) {
        message << "\n" << advisory << "\n";
    }

    // Additional info
    message << "\nEdition: " << result.GetWindowsEdition() << "\n";

    // KMS information (simplified for non-technical users)
    if (result.GetKmsStatus() == KMSStatus::KMSDetected) {
        if (!result.GetWindowsKmsServer().empty()) {
            message << "Activation: Enterprise Key Management Server\n";
        }
    }

    return message.str();
}

// Format Windows license status message
std::string NotificationFormatter::FormatWindowsStatus(const LicenseResult& result) {
    std::ostringstream status;

    status << "Windows License: " << GetStatusLabel(result.GetLicenseStatus());

    if (result.GetLicenseStatus() == LicenseStatus::UnableToDetermine) {
        status << " (Unable to verify)";
    }

    return status.str();
}

// Format advisory message for compliance issues
std::string NotificationFormatter::FormatAdvisory(LicenseStatus status) {
    switch (status) {
        case LicenseStatus::Legitimate:
            return "";  // No advisory for legitimate license

        case LicenseStatus::Cracked:
            return "⚠️ ADVISORY\n"
                   "This copy of Windows appears to be using an unauthorized license.\n"
                   "Please contact your IT support department to obtain a valid license.";

        case LicenseStatus::NotLicensed:
            return "⚠️ ADVISORY\n"
                   "This copy of Windows is not currently licensed.\n"
                   "Please activate Windows or contact your IT support department.";

        case LicenseStatus::UnableToDetermine:
            return "ℹ️ Unable to determine license status.\n"
                   "If you believe this is an error, please contact your IT support department.";

        default:
            return "";
    }
}

// Get user-friendly status label
std::string NotificationFormatter::GetStatusLabel(LicenseStatus status) {
    switch (status) {
        case LicenseStatus::Legitimate:
            return "✓ Legitimate (Properly Licensed)";

        case LicenseStatus::Cracked:
            return "✗ Cracked (Unauthorized License)";

        case LicenseStatus::NotLicensed:
            return "✗ Not Licensed (Activation Required)";

        case LicenseStatus::UnableToDetermine:
            return "? Unable to Determine";

        default:
            return "Unknown";
    }
}
