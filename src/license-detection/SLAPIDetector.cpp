#include "SLAPIDetector.h"
#include <windows.h>
#include <winerror.h>
#include <shlwapi.h>
#include <sstream>
#include <iostream>
#include <chrono>
#include <cstdlib>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "advapi32.lib")

SLAPIDetector::SLAPIDetector() {
}

SLAPIDetector::~SLAPIDetector() {
}

// Main detection method
LicenseResult SLAPIDetector::Detect() {
    LicenseResult result;
    auto timestamp = std::chrono::system_clock::now();

    try {
        // Detect Windows license status (FR-1, FR-2, FR-3)
        LicenseStatus windowsStatus = DetectWindowsLicenseStatus();
        result.SetLicenseStatus(windowsStatus);

        // Detect Windows version and edition (FR-4, FR-5)
        int version = 0;
        std::string edition = "";
        DetectWindowsVersionAndEdition(version, edition);
        result.SetWindowsVersion(version);
        result.SetWindowsEdition(edition);

        // Detect Windows KMS server if applicable (FR-9, FR-11)
        std::string windowsKmsServer = DetectWindowsKmsServer();
        if (!windowsKmsServer.empty()) {
            result.SetKmsStatus(KMSStatus::KMSDetected);
            result.SetWindowsKmsServer(windowsKmsServer);
        } else {
            result.SetKmsStatus(KMSStatus::NotKMS);
        }

        // Detect Office license status (FR-10)
        LicenseStatus officeStatus = DetectOfficeLicenseStatus();

        // Detect Office KMS server if applicable (FR-10, FR-11)
        std::string officeKmsServer = DetectOfficeKmsServer();
        result.SetOfficeKmsServer(officeKmsServer);

        result.SetTimestamp(timestamp);
        result.SetError(false);
    }
    catch (const std::exception& ex) {
        result.SetLicenseStatus(LicenseStatus::UnableToDetermine);
        result.SetError(true);
        result.SetErrorMessage(std::string("SL API detection failed: ") + ex.what());
        result.SetTimestamp(timestamp);
    }
    catch (...) {
        result.SetLicenseStatus(LicenseStatus::UnableToDetermine);
        result.SetError(true);
        result.SetErrorMessage("SL API detection failed: Unknown exception");
        result.SetTimestamp(timestamp);
    }

    return result;
}

// Detect Windows license status via SL APIs
LicenseStatus SLAPIDetector::DetectWindowsLicenseStatus() {
    // Query registry for license status
    // Windows license state is stored in registry: HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\SoftwareProtectionPlatform

    std::cout << "[DEBUG]   SL API: Opening registry key..." << std::endl;

    HKEY hKey = NULL;
    LONG result = RegOpenKeyExA(
        HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\SoftwareProtectionPlatform",
        0,
        KEY_READ,
        &hKey
    );

    if (result != ERROR_SUCCESS) {
        std::cout << "[DEBUG]   SL API: Registry open FAILED - Error code: " << result << std::endl;
        std::cout << "[DEBUG]   SL API: This usually means:" << std::endl;
        std::cout << "[DEBUG]   - Access Denied (not running as Admin?)" << std::endl;
        std::cout << "[DEBUG]   - Registry key not found (Windows licensing not initialized)" << std::endl;
        return LicenseStatus::UnableToDetermine;  // FR-6: API unavailable
    }

    std::cout << "[DEBUG]   SL API: Registry key opened successfully" << std::endl;

    LicenseStatus status = LicenseStatus::UnableToDetermine;

    try {
        // Query the license status from registry
        DWORD licenseStatus = 0;
        DWORD size = sizeof(licenseStatus);

        std::cout << "[DEBUG]   SL API: Querying LicenseStatus value..." << std::endl;

        LONG queryResult = RegQueryValueExA(
            hKey,
            "LicenseStatus",
            NULL,
            NULL,
            (LPBYTE)&licenseStatus,
            &size
        );

        if (queryResult == ERROR_SUCCESS) {
            std::cout << "[DEBUG]   SL API: LicenseStatus value found: " << licenseStatus << std::endl;
            // License Status codes:
            // 0 = Unlicensed
            // 1 = Initial grace period
            // 2 = Initial grace period with notification
            // 3 = Non-genuine grace period
            // 4 = Non-genuine grace period with notification
            // 5 = Out of tolerance grace period
            // 6 = Out of tolerance grace period with notification
            // 7 = Extended grace period
            // 8 = Extended grace period with notification
            // 10 = Genuine / Activated
            // 12 = Grace Period (aka Notification mode)

            if (licenseStatus == 0) {
                status = LicenseStatus::NotLicensed;  // Unlicensed (FR-2)
            }
            else if (licenseStatus >= 1 && licenseStatus <= 9) {
                status = LicenseStatus::NotLicensed;  // Grace period (FR-7)
            }
            else if (licenseStatus == 10) {
                status = LicenseStatus::Legitimate;  // Genuine/Activated (FR-2)
            }
            else if (licenseStatus == 12) {
                status = LicenseStatus::NotLicensed;  // Notification mode (FR-7)
            }
            else {
                // For other values, check if potentially non-genuine
                status = LicenseStatus::Cracked;  // Non-genuine states (FR-2)
            }
        }
        else {
            // Query succeeded but no LicenseStatus key found - try alternative detection
            std::cout << "[DEBUG]   SL API: LicenseStatus value NOT found - Error: " << queryResult << std::endl;
            status = LicenseStatus::UnableToDetermine;
        }
    }
    catch (...) {
        status = LicenseStatus::UnableToDetermine;
    }

    RegCloseKey(hKey);
    return status;
}

// Detect Windows version and edition
void SLAPIDetector::DetectWindowsVersionAndEdition(int& outVersion, std::string& outEdition) {
    outVersion = GetWindowsVersion();
    outEdition = GetWindowsEdition();
}

// Detect Windows KMS server address
std::string SLAPIDetector::DetectWindowsKmsServer() {
    // KMS server is stored in registry at:
    // HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\SoftwareProtectionPlatform\RPC\RPC

    return QueryRegistryForKmsServer(
        "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\SoftwareProtectionPlatform\\RPC"
    );
}

// Detect Office license status via SL APIs
LicenseStatus SLAPIDetector::DetectOfficeLicenseStatus() {
    // Office license detection would go here
    // For now, return NotLicensed if Office is not detected
    // This will be expanded in a future iteration

    return LicenseStatus::NotLicensed;  // Placeholder
}

// Detect Office KMS server address
std::string SLAPIDetector::DetectOfficeKmsServer() {
    // Office KMS server detection
    // Office stores KMS info in registry under various paths depending on version

    // Try common Office registry paths
    std::string paths[] = {
        "SOFTWARE\\Microsoft\\Office\\ClickToRun\\Configuration",  // Office 365 / Microsoft 365
        "SOFTWARE\\Microsoft\\Office\\16.0\\Common\\OEM",          // Office 2016
        "SOFTWARE\\Microsoft\\Office\\15.0\\Common\\OEM"           // Office 2013
    };

    for (const auto& path : paths) {
        std::string kmsServer = QueryRegistryForKmsServer(path);
        if (!kmsServer.empty()) {
            return kmsServer;
        }
    }

    return "";  // No KMS server found
}

// Query registry for KMS server address
std::string SLAPIDetector::QueryRegistryForKmsServer(const std::string& registryPath) {
    HKEY hKey = NULL;
    LONG result = RegOpenKeyExA(
        HKEY_LOCAL_MACHINE,
        registryPath.c_str(),
        0,
        KEY_READ,
        &hKey
    );

    if (result != ERROR_SUCCESS) {
        return "";  // Path doesn't exist
    }

    std::string kmsServer = "";

    try {
        // Try to read KMS machine name
        char buffer[256] = { 0 };
        DWORD size = sizeof(buffer);

        LONG queryResult = RegQueryValueExA(
            hKey,
            "KMSMachineName",
            NULL,
            NULL,
            (LPBYTE)buffer,
            &size
        );

        if (queryResult == ERROR_SUCCESS && size > 0) {
            kmsServer = std::string(buffer);
        }
    }
    catch (...) {
        // Ignore exceptions
    }

    RegCloseKey(hKey);
    return kmsServer;
}

// Get Windows version from OS
int SLAPIDetector::GetWindowsVersion() {
    // GetVersionEx() is subject to the OS's application-compatibility shim:
    // without a manifest declaring supportedOS entries, it reports a capped
    // "Windows 8" (6.2) to any process regardless of the real OS version.
    // Read the registry directly instead - it isn't shimmed.
    HKEY hKey = NULL;
    LONG openResult = RegOpenKeyExA(
        HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
        0,
        KEY_READ,
        &hKey
    );

    if (openResult == ERROR_SUCCESS) {
        DWORD majorVersion = 0;
        DWORD size = sizeof(majorVersion);
        LONG majorResult = RegQueryValueExA(
            hKey, "CurrentMajorVersionNumber", NULL, NULL,
            (LPBYTE)&majorVersion, &size
        );

        if (majorResult == ERROR_SUCCESS) {
            // Windows 11 shares major=10/minor=0 with Windows 10 - Microsoft
            // didn't bump the major version, so only the build number tells
            // them apart (>=22000 is Windows 11).
            char buildBuffer[32] = { 0 };
            DWORD buildSize = sizeof(buildBuffer);
            int buildNumber = 0;
            if (RegQueryValueExA(hKey, "CurrentBuildNumber", NULL, NULL,
                                  (LPBYTE)buildBuffer, &buildSize) == ERROR_SUCCESS) {
                buildNumber = atoi(buildBuffer);
            }

            RegCloseKey(hKey);

            if (majorVersion == 10 && buildNumber >= 22000) {
                return 11;  // Windows 11
            }
            if (majorVersion == 10) {
                return 10;  // Windows 10
            }
            return static_cast<int>(majorVersion);
        }

        // CurrentMajorVersionNumber doesn't exist pre-Windows 10; fall back
        // to the legacy "CurrentVersion" string ("6.1"/"6.2"/"6.3").
        char versionBuffer[32] = { 0 };
        DWORD versionSize = sizeof(versionBuffer);
        LONG versionResult = RegQueryValueExA(
            hKey, "CurrentVersion", NULL, NULL,
            (LPBYTE)versionBuffer, &versionSize
        );
        RegCloseKey(hKey);

        if (versionResult == ERROR_SUCCESS) {
            std::string version(versionBuffer);
            if (version == "6.1") return 7;        // Windows 7
            if (version == "6.2") return 8;        // Windows 8
            if (version == "6.3") return 81;       // Windows 8.1
        }
    }

    return 10;  // Default to Windows 10 if detection fails
}

// Get Windows edition/sku from OS
std::string SLAPIDetector::GetWindowsEdition() {
    // Query registry for Windows edition
    HKEY hKey = NULL;
    LONG result = RegOpenKeyExA(
        HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
        0,
        KEY_READ,
        &hKey
    );

    if (result != ERROR_SUCCESS) {
        return "Unknown";
    }

    std::string edition = "Unknown";

    try {
        char buffer[256] = { 0 };
        DWORD size = sizeof(buffer);

        // Try EditionID first
        LONG queryResult = RegQueryValueExA(
            hKey,
            "EditionID",
            NULL,
            NULL,
            (LPBYTE)buffer,
            &size
        );

        if (queryResult == ERROR_SUCCESS) {
            edition = std::string(buffer);
        }
        else {
            // Fallback to InstallationType
            size = sizeof(buffer);
            queryResult = RegQueryValueExA(
                hKey,
                "InstallationType",
                NULL,
                NULL,
                (LPBYTE)buffer,
                &size
            );

            if (queryResult == ERROR_SUCCESS) {
                edition = std::string(buffer);
            }
        }
    }
    catch (...) {
        // Ignore exceptions
    }

    RegCloseKey(hKey);
    return edition;
}
