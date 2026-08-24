#pragma once

#include "LicenseResult.h"
#include <string>

// Tier 3: Direct Windows Registry detector
// Uses registry inspection for license detection as fallback
class RegistryDetector {
public:
    RegistryDetector();
    ~RegistryDetector();

    // Delete copy semantics
    RegistryDetector(const RegistryDetector&) = delete;
    RegistryDetector& operator=(const RegistryDetector&) = delete;

    // Allow move semantics
    RegistryDetector(RegistryDetector&&) noexcept = default;
    RegistryDetector& operator=(RegistryDetector&&) noexcept = default;

    // Main detection method: queries SL APIs and returns results (FR-1)
    LicenseResult Detect();

private:
    // Helper: Detect Windows license status via SL APIs
    LicenseStatus DetectWindowsLicenseStatus(LicenseResult& result);

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

    // Helper: Query WMI SoftwareLicensingProduct for license status
    LicenseStatus QueryWMISoftwareLicensingProduct(LicenseResult& result);

    // Helper: Get Windows version from OS
    static int GetWindowsVersion();

    // Helper: Get Windows edition/sku from OS
    static std::string GetWindowsEdition();
};
