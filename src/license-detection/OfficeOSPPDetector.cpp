#include "OfficeOSPPDetector.h"
#include <windows.h>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <cstdlib>

namespace fs = std::filesystem;

OfficeOSPPDetector::OfficeOSPPDetector() {
    m_osppPath = FindOSPPScript();
}

OfficeOSPPDetector::~OfficeOSPPDetector() {
}

OfficeLicenseInfo OfficeOSPPDetector::Detect() {
    OfficeLicenseInfo info;

    if (m_osppPath.empty()) {
        return info;  // OSPP.VBS not found - Office not installed or is Microsoft 365
    }

    try {
        std::string command = "cscript.exe \"" + m_osppPath + "\" /dstatus";
        std::string output = ExecuteCommand(command);

        if (output.empty()) {
            return info;  // Empty output - OSPP might not be available
        }

        m_lastInfo = ParseOSPPOutput(output);
        return m_lastInfo;
    }
    catch (const std::exception& ex) {
        return info;
    }
    catch (...) {
        return info;
    }
}

std::string OfficeOSPPDetector::GetKmsServer() const {
    return m_lastInfo.kmsServer;
}

std::string OfficeOSPPDetector::ExecuteCommand(const std::string& command) {
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

std::string OfficeOSPPDetector::FindOSPPScript() {
    std::vector<std::string> searchPaths;

    // Get Program Files paths
    char* programFiles = nullptr;
    char* programFilesX86 = nullptr;
    size_t sz = 0;

    if (_dupenv_s(&programFiles, &sz, "ProgramFiles") == 0 && programFiles) {
        searchPaths.push_back(std::string(programFiles) + "\\Microsoft Office");
        free(programFiles);
    }

    if (_dupenv_s(&programFilesX86, &sz, "ProgramFiles(x86)") == 0 && programFilesX86) {
        searchPaths.push_back(std::string(programFilesX86) + "\\Microsoft Office");
        free(programFilesX86);
    }

    // Search for OSPP.VBS
    for (const auto& path : searchPaths) {
        try {
            for (const auto& entry : fs::recursive_directory_iterator(path)) {
                if (entry.path().filename() == "OSPP.VBS") {
                    return entry.path().string();
                }
            }
        }
        catch (...) {
            // Path doesn't exist or permission denied
            continue;
        }
    }

    return "";  // Not found
}

OfficeLicenseInfo OfficeOSPPDetector::ParseOSPPOutput(const std::string& output) {
    OfficeLicenseInfo info;

    try {
        // Parse detailed Office license info
    // Extract Product ID
    size_t pos = output.find("Product ID: ");
    if (pos != std::string::npos) {
        size_t end = output.find("\n", pos);
        if (end != std::string::npos) {
            info.productId = output.substr(pos + 12, end - pos - 12);
            size_t first = info.productId.find_first_not_of(" \t\r\n");
            if (first != std::string::npos) {
                info.productId = info.productId.substr(first);
                size_t last = info.productId.find_last_not_of(" \t\r\n");
                if (last != std::string::npos) info.productId = info.productId.substr(0, last + 1);
            }
        }
    }

    // Extract License Name
    pos = output.find("LICENSE NAME: ");
    if (pos != std::string::npos) {
        size_t end = output.find("\n", pos);
        if (end != std::string::npos) {
            info.licenseName = output.substr(pos + 14, end - pos - 14);
            size_t first = info.licenseName.find_first_not_of(" \t\r\n");
            if (first != std::string::npos) {
                info.licenseName = info.licenseName.substr(first);
                size_t last = info.licenseName.find_last_not_of(" \t\r\n");
                if (last != std::string::npos) info.licenseName = info.licenseName.substr(0, last + 1);
            }
        }
    }

    // Extract License Status
    pos = output.find("LICENSE STATUS: ");
    if (pos != std::string::npos) {
        size_t end = output.find("\n", pos);
        if (end != std::string::npos) {
            info.licenseStatus = output.substr(pos + 16, end - pos - 16);
            size_t first = info.licenseStatus.find_first_not_of(" \t\r\n");
            if (first != std::string::npos) {
                info.licenseStatus = info.licenseStatus.substr(first);
                size_t last = info.licenseStatus.find_last_not_of(" \t\r\n");
                if (last != std::string::npos) info.licenseStatus = info.licenseStatus.substr(0, last + 1);
            }
        }
    }

    // Extract Remaining Grace
    pos = output.find("REMAINING GRACE: ");
    if (pos != std::string::npos) {
        size_t end = output.find("\n", pos);
        if (end != std::string::npos) {
            info.remainingGrace = output.substr(pos + 17, end - pos - 17);
            size_t first = info.remainingGrace.find_first_not_of(" \t\r\n");
            if (first != std::string::npos) {
                info.remainingGrace = info.remainingGrace.substr(first);
                size_t last = info.remainingGrace.find_last_not_of(" \t\r\n");
                if (last != std::string::npos) info.remainingGrace = info.remainingGrace.substr(0, last + 1);
            }
        }
    }

    // Extract Last 5 characters of product key
    pos = output.find("Last 5 characters of installed product key: ");
    if (pos != std::string::npos) {
        size_t end = output.find("\n", pos);
        if (end != std::string::npos) {
            info.partialProductKey = output.substr(pos + 44, end - pos - 44);
            size_t first = info.partialProductKey.find_first_not_of(" \t\r\n");
            if (first != std::string::npos) {
                info.partialProductKey = info.partialProductKey.substr(first);
                size_t last = info.partialProductKey.find_last_not_of(" \t\r\n");
                if (last != std::string::npos) info.partialProductKey = info.partialProductKey.substr(0, last + 1);
            }
        }
    }

    // Extract KMS server (KMS machine registry override)
    pos = output.find("KMS machine registry override defined: ");
    if (pos != std::string::npos) {
        size_t end = output.find("\n", pos);
        if (end != std::string::npos) {
            info.kmsServer = output.substr(pos + 39, end - pos - 39);
            size_t first = info.kmsServer.find_first_not_of(" \t\r\n");
            if (first != std::string::npos) {
                info.kmsServer = info.kmsServer.substr(first);
                size_t last = info.kmsServer.find_last_not_of(" \t\r\n");
                if (last != std::string::npos) info.kmsServer = info.kmsServer.substr(0, last + 1);
            }
        }
    }

    // Extract Activation Interval
    pos = output.find("Activation Interval: ");
    if (pos != std::string::npos) {
        size_t end = output.find("\n", pos);
        if (end != std::string::npos) {
            info.activationInterval = output.substr(pos + 21, end - pos - 21);
            size_t first = info.activationInterval.find_first_not_of(" \t\r\n");
            if (first != std::string::npos) {
                info.activationInterval = info.activationInterval.substr(first);
                size_t last = info.activationInterval.find_last_not_of(" \t\r\n");
                if (last != std::string::npos) info.activationInterval = info.activationInterval.substr(0, last + 1);
            }
        }
    }

    return info;
    }
    catch (const std::exception& ex) {
        return info;  // Return empty info on error
    }
    catch (...) {
        return info;  // Return empty info on unknown error
    }
}
