#include "RegistryDetector.h"
#include <windows.h>
#include <wbemidl.h>
#include <comutil.h>
#include <winerror.h>
#include <shlwapi.h>
#include <sstream>
#include <iostream>
#include <chrono>
#include <cstdlib>

#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "comsuppwd.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "advapi32.lib")

RegistryDetector::RegistryDetector() {
}

RegistryDetector::~RegistryDetector() {
}

// Main detection method
LicenseResult RegistryDetector::Detect() {
    LicenseResult result;
    auto timestamp = std::chrono::system_clock::now();

    try {
        // Detect Windows license status (FR-1, FR-2, FR-3)
        LicenseStatus windowsStatus = DetectWindowsLicenseStatus(result);
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
        result.SetErrorMessage(std::string("Registry detection failed: ") + ex.what());
        result.SetTimestamp(timestamp);
    }
    catch (...) {
        result.SetLicenseStatus(LicenseStatus::UnableToDetermine);
        result.SetError(true);
        result.SetErrorMessage("Registry detection failed: Unknown exception");
        result.SetTimestamp(timestamp);
    }

    return result;
}

// Detect Windows license status via WMI SoftwareLicensingProduct
LicenseStatus RegistryDetector::DetectWindowsLicenseStatus(LicenseResult& result) {
    std::cout << "[DEBUG]   Registry: Trying WMI SoftwareLicensingProduct query first..." << std::endl;

    // Try WMI SoftwareLicensingProduct first (more reliable)
    LicenseStatus wmiStatus = QueryWMISoftwareLicensingProduct(result);
    if (wmiStatus != LicenseStatus::UnableToDetermine) {
        std::cout << "[DEBUG]   Registry: WMI query succeeded!" << std::endl;
        return wmiStatus;
    }

    std::cout << "[DEBUG]   Registry: WMI query failed, falling back to Registry..." << std::endl;

    // Fallback: Query registry for license status
    // Windows license state is stored in registry: HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\SoftwareProtectionPlatform

    HKEY hKey = NULL;
    LONG result_registry = RegOpenKeyExA(
        HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\SoftwareProtectionPlatform",
        0,
        KEY_READ,
        &hKey
    );

    if (result_registry != ERROR_SUCCESS) {
        std::cout << "[DEBUG]   Registry: Registry key not found" << std::endl;
        return LicenseStatus::UnableToDetermine;  // FR-6: API unavailable
    }

    LicenseStatus status = LicenseStatus::UnableToDetermine;

    try {
        // Query the license status from registry
        DWORD licenseStatus = 0;
        DWORD size = sizeof(licenseStatus);

        LONG queryResult = RegQueryValueExA(
            hKey,
            "LicenseStatus",
            NULL,
            NULL,
            (LPBYTE)&licenseStatus,
            &size
        );

        if (queryResult == ERROR_SUCCESS) {
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
void RegistryDetector::DetectWindowsVersionAndEdition(int& outVersion, std::string& outEdition) {
    outVersion = GetWindowsVersion();
    outEdition = GetWindowsEdition();
}

// Detect Windows KMS server address
std::string RegistryDetector::DetectWindowsKmsServer() {
    // KMS server is stored in registry at:
    // HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\SoftwareProtectionPlatform\RPC\RPC

    return QueryRegistryForKmsServer(
        "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\SoftwareProtectionPlatform\\RPC"
    );
}

// Detect Office license status via SL APIs
LicenseStatus RegistryDetector::DetectOfficeLicenseStatus() {
    // Office license detection would go here
    // For now, return NotLicensed if Office is not detected
    // This will be expanded in a future iteration

    return LicenseStatus::NotLicensed;  // Placeholder
}

// Detect Office KMS server address
std::string RegistryDetector::DetectOfficeKmsServer() {
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
std::string RegistryDetector::QueryRegistryForKmsServer(const std::string& registryPath) {
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

// Query WMI SoftwareLicensingProduct for license status
LicenseStatus RegistryDetector::QueryWMISoftwareLicensingProduct(LicenseResult& result) {
    try {
        std::cout << "[DEBUG]   WMI: Initializing COM..." << std::endl;

        HRESULT hres = CoInitializeEx(0, COINIT_MULTITHREADED);
        if (FAILED(hres)) {
            std::cout << "[DEBUG]   WMI: COM init failed - " << hres << std::endl;
            return LicenseStatus::UnableToDetermine;
        }

        hres = CoInitializeSecurity(
            NULL, -1, NULL, NULL,
            RPC_C_AUTHN_LEVEL_DEFAULT,
            RPC_C_IMP_LEVEL_IMPERSONATE,
            NULL, EOAC_NONE, NULL
        );

        // CoInitializeSecurity may only be called once per PROCESS (not per
        // thread/call) - every call after the first one ever made returns
        // RPC_E_TOO_LATE. That's not a real failure: security is already
        // configured from the earlier call, so treat it as success instead
        // of bailing out (which otherwise made every detection after the
        // first one in the process's lifetime report UnableToDetermine).
        if (FAILED(hres) && hres != RPC_E_TOO_LATE) {
            std::cout << "[DEBUG]   WMI: Security init failed - " << hres << std::endl;
            CoUninitialize();
            return LicenseStatus::UnableToDetermine;
        }

        IWbemLocator *pLocator = NULL;
        hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID *)&pLocator);

        if (FAILED(hres)) {
            std::cout << "[DEBUG]   WMI: Failed to create locator - " << hres << std::endl;
            CoUninitialize();
            return LicenseStatus::UnableToDetermine;
        }

        IWbemServices *pSvc = NULL;
        hres = pLocator->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0, 0, 0, 0, &pSvc);

        if (FAILED(hres)) {
            std::cout << "[DEBUG]   WMI: Failed to connect - " << hres << std::endl;
            pLocator->Release();
            CoUninitialize();
            return LicenseStatus::UnableToDetermine;
        }

        std::cout << "[DEBUG]   WMI: Connected to WMI service" << std::endl;

        IEnumWbemClassObject* pEnumerator = NULL;
        hres = pSvc->ExecQuery(
            _bstr_t("WQL"),
            _bstr_t("SELECT * FROM SoftwareLicensingProduct WHERE ApplicationID='55c92734-d682-4d71-983e-d6ec3f16059f' AND PartialProductKey IS NOT NULL"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            NULL,
            &pEnumerator
        );

        if (FAILED(hres)) {
            std::cout << "[DEBUG]   WMI: Query failed with hres=" << hres << std::endl;
            pSvc->Release();
            pLocator->Release();
            CoUninitialize();
            return LicenseStatus::UnableToDetermine;
        }

        std::cout << "[DEBUG]   WMI: Query executed" << std::endl;

        LicenseStatus status = LicenseStatus::UnableToDetermine;
        IWbemClassObject *pclsObj = NULL;
        ULONG uReturn = 0;
        int resultCount = 0;

        while (pEnumerator) {
            hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
            std::cout << "[DEBUG]   WMI: Next() returned hres=" << hres << ", uReturn=" << uReturn << std::endl;

            if (uReturn == 0) {
                std::cout << "[DEBUG]   WMI: No more results (uReturn=0)" << std::endl;
                break;
            }

            if (FAILED(hres)) {
                std::cout << "[DEBUG]   WMI: Next() failed with hres=" << hres << std::endl;
                break;
            }

            resultCount++;
            std::cout << "[DEBUG]   WMI: Processing result #" << resultCount << std::endl;

            WindowsLicenseInfo licenseInfo;
            VARIANT vtProp;
            VariantInit(&vtProp);

            // Get LicenseStatus
            hres = pclsObj->Get(L"LicenseStatus", 0, &vtProp, 0, 0);
            if (SUCCEEDED(hres)) {
                int licenseStatus = vtProp.iVal;
                std::cout << "[DEBUG]   WMI: LicenseStatus = " << licenseStatus << std::endl;

                if (licenseStatus == 1) {
                    status = LicenseStatus::Legitimate;
                    licenseInfo.licenseStatus = "Licensed";
                    std::cout << "[DEBUG]   ✓ WMI result: LICENSED" << std::endl;
                } else if (licenseStatus == 0) {
                    status = LicenseStatus::NotLicensed;
                    licenseInfo.licenseStatus = "NotLicensed";
                    std::cout << "[DEBUG]   ✗ WMI result: NOT LICENSED" << std::endl;
                } else if (licenseStatus >= 2 && licenseStatus <= 8) {
                    status = LicenseStatus::NotLicensed;
                    licenseInfo.licenseStatus = "GracePeriod";
                    std::cout << "[DEBUG]   ~ WMI result: GRACE PERIOD" << std::endl;
                } else {
                    status = LicenseStatus::Cracked;
                    licenseInfo.licenseStatus = "Cracked";
                    std::cout << "[DEBUG]   ✗ WMI result: CRACKED/NON-GENUINE" << std::endl;
                }
                VariantClear(&vtProp);
            } else {
                std::cout << "[DEBUG]   WMI: Failed to get LicenseStatus property" << std::endl;
            }

            // Get Name
            VariantInit(&vtProp);
            hres = pclsObj->Get(L"Name", 0, &vtProp, 0, 0);
            if (SUCCEEDED(hres) && vtProp.vt == VT_BSTR) {
                licenseInfo.name = (const char*)_bstr_t(vtProp.bstrVal);
                std::cout << "[DEBUG]   WMI: Name = " << licenseInfo.name << std::endl;
                VariantClear(&vtProp);
            }

            // Get Description
            VariantInit(&vtProp);
            hres = pclsObj->Get(L"Description", 0, &vtProp, 0, 0);
            if (SUCCEEDED(hres) && vtProp.vt == VT_BSTR) {
                licenseInfo.description = (const char*)_bstr_t(vtProp.bstrVal);
                std::cout << "[DEBUG]   WMI: Description = " << licenseInfo.description << std::endl;
                VariantClear(&vtProp);
            }

            // Get PartialProductKey
            VariantInit(&vtProp);
            hres = pclsObj->Get(L"PartialProductKey", 0, &vtProp, 0, 0);
            if (SUCCEEDED(hres) && vtProp.vt == VT_BSTR) {
                licenseInfo.partialProductKey = (const char*)_bstr_t(vtProp.bstrVal);
                std::cout << "[DEBUG]   WMI: PartialProductKey = " << licenseInfo.partialProductKey << std::endl;
                VariantClear(&vtProp);
            }

            // Get ProductKeyChannel
            VariantInit(&vtProp);
            hres = pclsObj->Get(L"ProductKeyChannel", 0, &vtProp, 0, 0);
            if (SUCCEEDED(hres) && vtProp.vt == VT_BSTR) {
                licenseInfo.productKeyChannel = (const char*)_bstr_t(vtProp.bstrVal);
                std::cout << "[DEBUG]   WMI: ProductKeyChannel = " << licenseInfo.productKeyChannel << std::endl;
                VariantClear(&vtProp);
            }

            // Get ActivationId
            VariantInit(&vtProp);
            hres = pclsObj->Get(L"ActivationId", 0, &vtProp, 0, 0);
            if (SUCCEEDED(hres) && vtProp.vt == VT_BSTR) {
                licenseInfo.activationId = (const char*)_bstr_t(vtProp.bstrVal);
                std::cout << "[DEBUG]   WMI: ActivationId = " << licenseInfo.activationId << std::endl;
                VariantClear(&vtProp);
            }

            // Get ApplicationId
            VariantInit(&vtProp);
            hres = pclsObj->Get(L"ApplicationId", 0, &vtProp, 0, 0);
            if (SUCCEEDED(hres) && vtProp.vt == VT_BSTR) {
                licenseInfo.applicationId = (const char*)_bstr_t(vtProp.bstrVal);
                std::cout << "[DEBUG]   WMI: ApplicationId = " << licenseInfo.applicationId << std::endl;
                VariantClear(&vtProp);
            }

            // Get ExtendedPID
            VariantInit(&vtProp);
            hres = pclsObj->Get(L"ExtendedPID", 0, &vtProp, 0, 0);
            if (SUCCEEDED(hres) && vtProp.vt == VT_BSTR) {
                licenseInfo.extendedPid = (const char*)_bstr_t(vtProp.bstrVal);
                std::cout << "[DEBUG]   WMI: ExtendedPID = " << licenseInfo.extendedPid << std::endl;
                VariantClear(&vtProp);
            }

            // Get InstallationId
            VariantInit(&vtProp);
            hres = pclsObj->Get(L"InstallationId", 0, &vtProp, 0, 0);
            if (SUCCEEDED(hres) && vtProp.vt == VT_BSTR) {
                licenseInfo.installationId = (const char*)_bstr_t(vtProp.bstrVal);
                std::cout << "[DEBUG]   WMI: InstallationId = " << licenseInfo.installationId << std::endl;
                VariantClear(&vtProp);
            }

            // Get UseLetURL
            VariantInit(&vtProp);
            hres = pclsObj->Get(L"UseLicenseURL", 0, &vtProp, 0, 0);
            if (SUCCEEDED(hres) && vtProp.vt == VT_BSTR) {
                licenseInfo.useLicenseUrl = (const char*)_bstr_t(vtProp.bstrVal);
                std::cout << "[DEBUG]   WMI: UseLicenseURL = " << licenseInfo.useLicenseUrl << std::endl;
                VariantClear(&vtProp);
            }

            // Get ValidationURL
            VariantInit(&vtProp);
            hres = pclsObj->Get(L"ValidationURL", 0, &vtProp, 0, 0);
            if (SUCCEEDED(hres) && vtProp.vt == VT_BSTR) {
                licenseInfo.validationUrl = (const char*)_bstr_t(vtProp.bstrVal);
                std::cout << "[DEBUG]   WMI: ValidationURL = " << licenseInfo.validationUrl << std::endl;
                VariantClear(&vtProp);
            }

            // Get InitialPromotionCodeNotificationSent (check for rearm count)
            VariantInit(&vtProp);
            hres = pclsObj->Get(L"RemainingRearmCount", 0, &vtProp, 0, 0);
            if (SUCCEEDED(hres) && vtProp.vt == VT_I4) {
                licenseInfo.remainingRearmCount = vtProp.intVal;
                std::cout << "[DEBUG]   WMI: RemainingRearmCount = " << licenseInfo.remainingRearmCount << std::endl;
                VariantClear(&vtProp);
            }

            // Get TrustedTime
            VariantInit(&vtProp);
            hres = pclsObj->Get(L"TrustedTime", 0, &vtProp, 0, 0);
            if (SUCCEEDED(hres) && vtProp.vt == VT_BSTR) {
                licenseInfo.trustedTime = (const char*)_bstr_t(vtProp.bstrVal);
                std::cout << "[DEBUG]   WMI: TrustedTime = " << licenseInfo.trustedTime << std::endl;
                VariantClear(&vtProp);
            }

            // Set Windows license info to result
            result.SetWindowsLicenseInfo(licenseInfo);

            pclsObj->Release();
        }

        std::cout << "[DEBUG]   WMI: Total results processed: " << resultCount << std::endl;

        pEnumerator->Release();
        pSvc->Release();
        pLocator->Release();
        CoUninitialize();

        return status;
    }
    catch (...) {
        std::cout << "[DEBUG]   WMI: Exception occurred" << std::endl;
        CoUninitialize();
        return LicenseStatus::UnableToDetermine;
    }
}

// Get Windows version from OS
int RegistryDetector::GetWindowsVersion() {
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
std::string RegistryDetector::GetWindowsEdition() {
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
