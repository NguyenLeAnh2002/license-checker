#include "SharedLicenseDataWriter.h"
#include "../license-detection/LicenseResult.h"
#include "../license-detection/LicenseStatusEnum.h"
#include <windows.h>
#include <sstream>
#include <iomanip>
#include <ctime>

const char* REGISTRY_PATH = "SOFTWARE\\LicenseCheckerAgent";

std::string SharedLicenseDataWriter::TimestampToISO8601(std::time_t timestamp)
{
    struct tm timeinfo;
    localtime_s(&timeinfo, &timestamp);
    char buffer[30];
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
    return std::string(buffer);
}

bool SharedLicenseDataWriter::SetRegistryValue(const char* valueName, const std::string& jsonData)
{
    HKEY hKey;
    LONG result = RegOpenKeyExA(HKEY_LOCAL_MACHINE, REGISTRY_PATH, 0, KEY_WRITE, &hKey);

    if (result != ERROR_SUCCESS) {
        // Try to create the key if it doesn't exist
        result = RegCreateKeyExA(HKEY_LOCAL_MACHINE, REGISTRY_PATH, 0, nullptr,
                               REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);
        if (result != ERROR_SUCCESS) {
            return false;
        }
    }

    result = RegSetValueExA(hKey, valueName, 0, REG_SZ,
                           reinterpret_cast<const BYTE*>(jsonData.c_str()),
                           static_cast<DWORD>(jsonData.length() + 1));

    RegCloseKey(hKey);
    return result == ERROR_SUCCESS;
}

void SharedLicenseDataWriter::WriteLicenseDataToRegistry(const LicenseResult& result)
{
    std::ostringstream json;

    json << "{";
    json << "\"edition\":\"" << result.GetWindowsEdition() << "\",";
    json << "\"licenseStatus\":" << static_cast<int>(result.GetLicenseStatus()) << ",";
    json << "\"kmsStatus\":" << static_cast<int>(result.GetKmsStatus()) << ",";
    json << "\"kmsServer\":\"" << result.GetWindowsKmsServer() << "\",";
    json << "\"gracePeriod\":\"\",";

    // Convert timestamp to ISO8601
    std::time_t timestamp = std::chrono::system_clock::to_time_t(result.GetTimestamp());
    std::string timestampStr = TimestampToISO8601(timestamp);
    json << "\"lastDetected\":\"" << timestampStr << "\"";

    json << "}";

    SetRegistryValue("WindowsLicense", json.str());

    // Also update the timestamp of last detection
    std::ostringstream tsJson;
    tsJson << "{\"timestamp\":\"" << timestampStr << "\"}";
    SetRegistryValue("LastDetectionTime", tsJson.str());
}

void SharedLicenseDataWriter::WriteSystemInfoToRegistry(const std::string& machineGuid, const std::string& hostname)
{
    std::ostringstream json;
    json << "{";
    json << "\"machineGuid\":\"" << machineGuid << "\",";
    json << "\"hostname\":\"" << hostname << "\"";
    json << "}";

    SetRegistryValue("SystemInfo", json.str());
}

void SharedLicenseDataWriter::WriteServiceStatusToRegistry(bool isRunning)
{
    std::ostringstream json;
    json << "{";
    json << "\"serviceRunning\":" << (isRunning ? "true" : "false") << ",";

    std::time_t now = std::time(nullptr);
    json << "\"lastCheckIn\":\"" << TimestampToISO8601(now) << "\"";

    json << "}";

    SetRegistryValue("ServiceStatus", json.str());
}
