#include "WindowsSLMgrDetector.h"
#include <windows.h>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <winreg.h>

WindowsSLMgrDetector::WindowsSLMgrDetector() {
}

WindowsSLMgrDetector::~WindowsSLMgrDetector() {
}

LicenseResult WindowsSLMgrDetector::Detect() {
    LicenseResult result;

    try {
        // Execute slmgr.vbs /dlv
        const char* sysRoot = std::getenv("SystemRoot");
        if (!sysRoot) {
            result.SetError(true);
            result.SetErrorMessage("SystemRoot environment variable not found");
            return result;
        }

        std::string command = "cscript.exe \"" + std::string(sysRoot) + "\\System32\\slmgr.vbs\" /dlv";
        std::string output = ExecuteCommand(command);

        if (output.empty()) {
            result.SetError(true);
            result.SetErrorMessage("Failed to execute slmgr.vbs - empty output");
            return result;
        }

        result = ParseSLMgrOutput(output);
        result.SetWindowsVersion(GetWindowsVersion());
        result.SetWindowsEdition(GetWindowsEdition());

        return result;
    }
    catch (const std::exception& ex) {
        result.SetError(true);
        result.SetErrorMessage(std::string("Exception: ") + ex.what());
        return result;
    }
    catch (...) {
        result.SetError(true);
        result.SetErrorMessage("Unknown exception in WindowsSLMgrDetector::Detect");
        return result;
    }
}

std::string WindowsSLMgrDetector::ExecuteCommand(const std::string& command) {
    FILE* pipe = _popen(command.c_str(), "r");
    if (!pipe) {
        return "";
    }

    std::string result;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }

    _pclose(pipe);
    return result;
}

LicenseResult WindowsSLMgrDetector::ParseSLMgrOutput(const std::string& output) {
    LicenseResult result;
    WindowsLicenseInfo winInfo;

    try {
        // Parse detailed Windows license info
        // Extract Name
    size_t pos = output.find("Name: ");
    if (pos != std::string::npos) {
        size_t end = output.find("\n", pos);
        if (end != std::string::npos) {
            winInfo.name = output.substr(pos + 6, end - pos - 6);
            size_t first = winInfo.name.find_first_not_of(" \t\r\n");
            if (first != std::string::npos) {
                winInfo.name = winInfo.name.substr(first);
                size_t last = winInfo.name.find_last_not_of(" \t\r\n");
                if (last != std::string::npos) winInfo.name = winInfo.name.substr(0, last + 1);
            }
        }
    }

    // Extract Description
    pos = output.find("Description: ");
    if (pos != std::string::npos) {
        size_t end = output.find("\n", pos);
        if (end != std::string::npos) {
            winInfo.description = output.substr(pos + 13, end - pos - 13);
            size_t first = winInfo.description.find_first_not_of(" \t\r\n");
            if (first != std::string::npos) {
                winInfo.description = winInfo.description.substr(first);
                size_t last = winInfo.description.find_last_not_of(" \t\r\n");
                if (last != std::string::npos) winInfo.description = winInfo.description.substr(0, last + 1);
            }
        }
    }

    // Extract Partial Product Key
    pos = output.find("Partial Product Key: ");
    if (pos != std::string::npos) {
        size_t end = output.find("\n", pos);
        if (end != std::string::npos) {
            winInfo.partialProductKey = output.substr(pos + 21, end - pos - 21);
            size_t first = winInfo.partialProductKey.find_first_not_of(" \t\r\n");
            if (first != std::string::npos) {
                winInfo.partialProductKey = winInfo.partialProductKey.substr(first);
                size_t last = winInfo.partialProductKey.find_last_not_of(" \t\r\n");
                if (last != std::string::npos) winInfo.partialProductKey = winInfo.partialProductKey.substr(0, last + 1);
            }
        }
    }

    // Extract License Status
    pos = output.find("License Status: ");
    if (pos != std::string::npos) {
        size_t end = output.find("\n", pos);
        if (end != std::string::npos) {
            winInfo.licenseStatus = output.substr(pos + 15, end - pos - 15);
            size_t first = winInfo.licenseStatus.find_first_not_of(" \t\r\n");
            if (first != std::string::npos) {
                winInfo.licenseStatus = winInfo.licenseStatus.substr(first);
                size_t last = winInfo.licenseStatus.find_last_not_of(" \t\r\n");
                if (last != std::string::npos) winInfo.licenseStatus = winInfo.licenseStatus.substr(0, last + 1);
            }
        }
    }

    // Set license status based on parsed info
    if (winInfo.licenseStatus.find("Licensed") != std::string::npos) {
        result.SetLicenseStatus(LicenseStatus::Legitimate);
    } else if (winInfo.licenseStatus.find("grace") != std::string::npos ||
               winInfo.licenseStatus.find("Initial") != std::string::npos) {
        result.SetLicenseStatus(LicenseStatus::NotLicensed);
    } else {
        result.SetLicenseStatus(LicenseStatus::UnableToDetermine);
    }

    // Extract KMS info
    pos = output.find("KMS machine");
    if (pos != std::string::npos) {
        result.SetKmsStatus(KMSStatus::KMSDetected);
        size_t start = output.find(":", pos) + 1;
        size_t end = output.find("\n", start);
        if (start != std::string::npos && end != std::string::npos) {
            std::string server = output.substr(start, end - start);
            size_t first = server.find_first_not_of(" \t\r\n");
            size_t last = server.find_last_not_of(" \t\r\n");
            if (first != std::string::npos) {
                server = server.substr(first, last - first + 1);
                result.SetWindowsKmsServer(server);
            }
        }
    } else {
        result.SetKmsStatus(KMSStatus::NotKMS);
    }

    // Set detailed Windows info
    result.SetWindowsLicenseInfo(winInfo);

    return result;
    }
    catch (const std::exception& ex) {
        result.SetError(true);
        result.SetErrorMessage(std::string("Parsing error: ") + ex.what());
        return result;
    }
    catch (...) {
        result.SetError(true);
        result.SetErrorMessage("Unknown error during parsing");
        return result;
    }
}

int WindowsSLMgrDetector::GetWindowsVersion() {
    // Get Windows version from registry or WMI
    // For now, return 10 (Windows 10)
    // This can be enhanced to detect actual version
    return 10;
}

std::string WindowsSLMgrDetector::GetWindowsEdition() {
    // Get Windows edition from registry or system
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
        0, KEY_READ, &hKey) == ERROR_SUCCESS) {

        char edition[256] = {0};
        DWORD size = sizeof(edition);

        if (RegQueryValueExA(hKey, "EditionID", NULL, NULL, (LPBYTE)edition, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return std::string(edition);
        }

        RegCloseKey(hKey);
    }

    return "";
}
