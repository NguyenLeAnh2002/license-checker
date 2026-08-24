#pragma once

#include <string>
#include <chrono>
#include "LicenseStatusEnum.h"
#include "LicenseInfo.h"

// Encapsulates the result of a license detection operation (FR-13, FR-14)
class LicenseResult {
public:
    // Constructors
    LicenseResult();

    LicenseResult(
        LicenseStatus licenseStatus,
        std::chrono::system_clock::time_point timestamp,
        int windowsVersion,
        const std::string& windowsEdition,
        KMSStatus kmsStatus,
        const std::string& windowsKmsServer,
        const std::string& officeKmsServer,
        bool isError
    );

    // Destructor
    ~LicenseResult() = default;

    // Copy semantics
    LicenseResult(const LicenseResult& other) = default;
    LicenseResult& operator=(const LicenseResult& other) = default;

    // Move semantics
    LicenseResult(LicenseResult&& other) noexcept = default;
    LicenseResult& operator=(LicenseResult&& other) noexcept = default;

    // Getters
    LicenseStatus GetLicenseStatus() const;
    std::chrono::system_clock::time_point GetTimestamp() const;
    int GetWindowsVersion() const;
    const std::string& GetWindowsEdition() const;
    KMSStatus GetKmsStatus() const;
    const std::string& GetWindowsKmsServer() const;
    const std::string& GetOfficeKmsServer() const;
    bool IsError() const;
    const std::string& GetErrorMessage() const;

    // Detailed info getters
    const WindowsLicenseInfo& GetWindowsLicenseInfo() const;
    const OfficeLicenseInfo& GetOfficeLicenseInfo() const;

    // Setters
    void SetLicenseStatus(LicenseStatus status);
    void SetTimestamp(std::chrono::system_clock::time_point timestamp);
    void SetWindowsVersion(int version);
    void SetWindowsEdition(const std::string& edition);
    void SetKmsStatus(KMSStatus status);
    void SetWindowsKmsServer(const std::string& server);
    void SetOfficeKmsServer(const std::string& server);
    void SetError(bool error);
    void SetErrorMessage(const std::string& message);

    // Detailed info setters
    void SetWindowsLicenseInfo(const WindowsLicenseInfo& info);
    void SetOfficeLicenseInfo(const OfficeLicenseInfo& info);

    // JSON serialization (FR-13, FR-14)
    std::string ToJSON() const;

private:
    LicenseStatus licenseStatus_;
    std::chrono::system_clock::time_point timestamp_;
    int windowsVersion_;
    std::string windowsEdition_;
    KMSStatus kmsStatus_;
    std::string windowsKmsServer_;
    std::string officeKmsServer_;
    bool isError_;
    std::string errorMessage_;
    WindowsLicenseInfo windowsLicenseInfo_;
    OfficeLicenseInfo officeLicenseInfo_;
};
