#pragma once

#include <string>

// Windows Service (de)registration/control helpers for the LicenseCheckerAgent
// service. Reused by both the agent itself (not currently) and Installer.exe.

bool InstallService(const std::string& serviceName, const std::string& displayName, const std::string& exePath);
bool UninstallService(const std::string& serviceName);
bool StartServiceNow(const std::string& serviceName);
bool StopServiceNow(const std::string& serviceName);
