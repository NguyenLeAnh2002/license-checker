#pragma once

#include <string>

// License status enumeration (FR-2, FR-6)
enum class LicenseStatus {
    Legitimate,          // Windows is properly licensed and activated
    Cracked,             // Windows is using an unauthorized/pirated license or has been tampered with
    NotLicensed,         // Windows is installed but has never been licensed/activated
    UnableToDetermine    // Detection failed; status cannot be determined
};

// KMS (Key Management Service) status enumeration (FR-9, FR-10, FR-11)
enum class KMSStatus {
    NotKMS,      // Not activated via KMS
    KMSDetected, // KMS activation detected
    KMSNotFound, // KMS not found (tried but unavailable)
    Error        // Error detecting KMS status
};

// Windows version constants (FR-4, FR-5)
constexpr int WINDOWS_VERSION_7 = 7;
constexpr int WINDOWS_VERSION_8 = 8;
constexpr int WINDOWS_VERSION_81 = 81;
constexpr int WINDOWS_VERSION_10 = 10;
constexpr int WINDOWS_VERSION_11 = 11;
constexpr int WINDOWS_SERVER_2008 = 2008;
constexpr int WINDOWS_SERVER_2012 = 2012;
constexpr int WINDOWS_SERVER_2016 = 2016;
constexpr int WINDOWS_SERVER_2019 = 2019;
constexpr int WINDOWS_SERVER_2022 = 2022;

// Enum-to-string conversion functions
inline std::string LicenseStatusToString(LicenseStatus status) {
    switch (status) {
        case LicenseStatus::Legitimate:
            return "Legitimate";
        case LicenseStatus::Cracked:
            return "Cracked";
        case LicenseStatus::NotLicensed:
            return "NotLicensed";
        case LicenseStatus::UnableToDetermine:
            return "UnableToDetermine";
        default:
            return "Unknown";
    }
}

inline std::string KMSStatusToString(KMSStatus status) {
    switch (status) {
        case KMSStatus::NotKMS:
            return "NotKMS";
        case KMSStatus::KMSDetected:
            return "KMSDetected";
        case KMSStatus::KMSNotFound:
            return "KMSNotFound";
        case KMSStatus::Error:
            return "Error";
        default:
            return "Unknown";
    }
}

// String-to-enum conversion functions (useful for parsing/validation)
inline LicenseStatus StringToLicenseStatus(const std::string& str) {
    if (str == "Legitimate") return LicenseStatus::Legitimate;
    if (str == "Cracked") return LicenseStatus::Cracked;
    if (str == "NotLicensed") return LicenseStatus::NotLicensed;
    if (str == "UnableToDetermine") return LicenseStatus::UnableToDetermine;
    return LicenseStatus::UnableToDetermine; // Default to unknown state
}

inline KMSStatus StringToKMSStatus(const std::string& str) {
    if (str == "NotKMS") return KMSStatus::NotKMS;
    if (str == "KMSDetected") return KMSStatus::KMSDetected;
    if (str == "KMSNotFound") return KMSStatus::KMSNotFound;
    if (str == "Error") return KMSStatus::Error;
    return KMSStatus::Error; // Default to error state
}
