#pragma once

#include "LicenseResult.h"
#include <string>

// Tier 2: Windows Management Instrumentation (WMI) detector
// Uses WMI for license detection on Windows 7/8 where SL APIs may be unavailable
class WMIDetector {
public:
    WMIDetector();
    ~WMIDetector();

    // Delete copy semantics
    WMIDetector(const WMIDetector&) = delete;
    WMIDetector& operator=(const WMIDetector&) = delete;

    // Allow move semantics
    WMIDetector(WMIDetector&&) noexcept = default;
    WMIDetector& operator=(WMIDetector&&) noexcept = default;

    // Main detection method: queries SL APIs and returns results (FR-1)
    LicenseResult Detect();

private:
    // Helper: Detect Windows license status via SL APIs
    LicenseStatus DetectWindowsLicenseStatus();

    // Helper: Detect Windows version and edition
    void DetectWindowsVersionAndEdition(int& outVersion, std::string& outEdition);

    // Helper: Detect KMS server address for Windows
    std::string DetectWindowsKmsServer();

    // Helper: Detect Office license status via SL APIs
    LicenseStatus DetectOfficeLicenseStatus();

    // Helper: Detect KMS server address for Office
    std::string DetectOfficeKmsServer();

    // Helper: Query registry for KMS server address
    static std::string QueryRegistryForKmsServer(const std::string& registryPath);

    // Helper: Get Windows version from OS
    static int GetWindowsVersion();

    // Helper: Get Windows edition/sku from OS
    static std::string GetWindowsEdition();
};
