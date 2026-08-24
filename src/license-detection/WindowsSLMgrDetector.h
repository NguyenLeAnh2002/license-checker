#pragma once

#include <string>
#include "LicenseResult.h"

class WindowsSLMgrDetector {
public:
    WindowsSLMgrDetector();
    ~WindowsSLMgrDetector();

    // Detect Windows license using slmgr.vbs /dlv
    LicenseResult Detect();

private:
    std::string ExecuteCommand(const std::string& command);
    LicenseResult ParseSLMgrOutput(const std::string& output);

    int GetWindowsVersion();
    std::string GetWindowsEdition();
};
