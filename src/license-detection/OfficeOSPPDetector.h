#pragma once

#include <string>
#include "LicenseInfo.h"

class OfficeOSPPDetector {
public:
    OfficeOSPPDetector();
    ~OfficeOSPPDetector();

    // Detect Office license using OSPP.VBS /dstatus
    OfficeLicenseInfo Detect();

    // Get KMS server for backward compatibility
    std::string GetKmsServer() const;

private:
    std::string ExecuteCommand(const std::string& command);
    std::string FindOSPPScript();
    OfficeLicenseInfo ParseOSPPOutput(const std::string& output);

    std::string m_osppPath;
    OfficeLicenseInfo m_lastInfo;
};
