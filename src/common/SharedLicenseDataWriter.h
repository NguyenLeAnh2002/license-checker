#ifndef SHAREDLICENSEDATAWRITER_H
#define SHAREDLICENSEDATAWRITER_H

#include <string>
#include <ctime>

class LicenseResult;

class SharedLicenseDataWriter {
public:
    static void WriteLicenseDataToRegistry(const LicenseResult& result);
    static void WriteSystemInfoToRegistry(const std::string& machineGuid, const std::string& hostname);
    static void WriteServiceStatusToRegistry(bool isRunning);

private:
    static std::string TimestampToISO8601(std::time_t timestamp);
    static bool SetRegistryValue(const char* valueName, const std::string& jsonData);
};

#endif // SHAREDLICENSEDATAWRITER_H
