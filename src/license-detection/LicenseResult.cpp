#include "LicenseResult.h"
#include <sstream>
#include <iomanip>
#include <ctime>

// Default constructor
LicenseResult::LicenseResult()
    : licenseStatus_(LicenseStatus::UnableToDetermine),
      timestamp_(std::chrono::system_clock::now()),
      windowsVersion_(0),
      windowsEdition_(""),
      kmsStatus_(KMSStatus::NotKMS),
      windowsKmsServer_(""),
      officeKmsServer_(""),
      isError_(false),
      errorMessage_("") {
}

// Parameterized constructor
LicenseResult::LicenseResult(
    LicenseStatus licenseStatus,
    std::chrono::system_clock::time_point timestamp,
    int windowsVersion,
    const std::string& windowsEdition,
    KMSStatus kmsStatus,
    const std::string& windowsKmsServer,
    const std::string& officeKmsServer,
    bool isError
)
    : licenseStatus_(licenseStatus),
      timestamp_(timestamp),
      windowsVersion_(windowsVersion),
      windowsEdition_(windowsEdition),
      kmsStatus_(kmsStatus),
      windowsKmsServer_(windowsKmsServer),
      officeKmsServer_(officeKmsServer),
      isError_(isError),
      errorMessage_("") {
}

// Getters
LicenseStatus LicenseResult::GetLicenseStatus() const {
    return licenseStatus_;
}

std::chrono::system_clock::time_point LicenseResult::GetTimestamp() const {
    return timestamp_;
}

int LicenseResult::GetWindowsVersion() const {
    return windowsVersion_;
}

const std::string& LicenseResult::GetWindowsEdition() const {
    return windowsEdition_;
}

KMSStatus LicenseResult::GetKmsStatus() const {
    return kmsStatus_;
}

const std::string& LicenseResult::GetWindowsKmsServer() const {
    return windowsKmsServer_;
}

const std::string& LicenseResult::GetOfficeKmsServer() const {
    return officeKmsServer_;
}

bool LicenseResult::IsError() const {
    return isError_;
}

const std::string& LicenseResult::GetErrorMessage() const {
    return errorMessage_;
}

// Setters
void LicenseResult::SetLicenseStatus(LicenseStatus status) {
    licenseStatus_ = status;
}

void LicenseResult::SetTimestamp(std::chrono::system_clock::time_point timestamp) {
    timestamp_ = timestamp;
}

void LicenseResult::SetWindowsVersion(int version) {
    windowsVersion_ = version;
}

void LicenseResult::SetWindowsEdition(const std::string& edition) {
    windowsEdition_ = edition;
}

void LicenseResult::SetKmsStatus(KMSStatus status) {
    kmsStatus_ = status;
}

void LicenseResult::SetWindowsKmsServer(const std::string& server) {
    windowsKmsServer_ = server;
}

void LicenseResult::SetOfficeKmsServer(const std::string& server) {
    officeKmsServer_ = server;
}

void LicenseResult::SetError(bool error) {
    isError_ = error;
}

void LicenseResult::SetErrorMessage(const std::string& message) {
    errorMessage_ = message;
}

const WindowsLicenseInfo& LicenseResult::GetWindowsLicenseInfo() const {
    return windowsLicenseInfo_;
}

const OfficeLicenseInfo& LicenseResult::GetOfficeLicenseInfo() const {
    return officeLicenseInfo_;
}

void LicenseResult::SetWindowsLicenseInfo(const WindowsLicenseInfo& info) {
    windowsLicenseInfo_ = info;
}

void LicenseResult::SetOfficeLicenseInfo(const OfficeLicenseInfo& info) {
    officeLicenseInfo_ = info;
}

// JSON serialization
std::string LicenseResult::ToJSON() const {
    std::ostringstream json;

    // Format timestamp as ISO 8601 string
    auto time_t_now = std::chrono::system_clock::to_time_t(timestamp_);
    std::ostringstream timestamp_str;
    timestamp_str << std::put_time(std::localtime(&time_t_now), "%Y-%m-%dT%H:%M:%S");

    // Build JSON object
    json << "{"
         << "\"version\": \"1.0\","
         << "\"licenseStatus\": \"" << LicenseStatusToString(licenseStatus_) << "\","
         << "\"timestamp\": \"" << timestamp_str.str() << "\","
         << "\"windowsVersion\": " << windowsVersion_ << ","
         << "\"windowsEdition\": \"" << windowsEdition_ << "\","
         << "\"kmsStatus\": \"" << KMSStatusToString(kmsStatus_) << "\","
         << "\"windowsKmsServer\": \"" << windowsKmsServer_ << "\","
         << "\"officeKmsServer\": \"" << officeKmsServer_ << "\","
         << "\"isError\": " << (isError_ ? "true" : "false") << ","
         << "\"errorMessage\": \"" << errorMessage_ << "\""
         << "}";

    return json.str();
}
