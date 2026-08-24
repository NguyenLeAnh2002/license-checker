#pragma once

#include "LicenseResult.h"
#include <string>

// Tier 1: Windows Software Licensing (SL) API detector
// Uses native Windows licensing APIs for fast, accurate detection on Windows 10/11/Server
class SLAPIDetector {
public:
    SLAPIDetector();
    ~SLAPIDetector();

    // Delete copy semantics
    SLAPIDetector(const SLAPIDetector&) = delete;
    SLAPIDetector& operator=(const SLAPIDetector&) = delete;

    // Allow move semantics
    SLAPIDetector(SLAPIDetector&&) noexcept = default;
    SLAPIDetector& operator=(SLAPIDetector&&) noexcept = default;

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
