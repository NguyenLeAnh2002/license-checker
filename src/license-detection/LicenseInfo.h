#pragma once

#include <string>
#include <map>

// Detailed Windows License Information
struct WindowsLicenseInfo {
    std::string name;                      // Windows(R) Operating System
    std::string description;               // Description
    std::string edition;                   // Edition (CoreSingleLanguage, etc)
    std::string licenseStatus;             // Licensed, Not Licensed, etc
    std::string activationId;              // Activation ID
    std::string applicationId;             // Application ID
    std::string extendedPid;               // Extended PID
    std::string productKeyChannel;         // OEM:DM, etc
    std::string installationId;            // Installation ID
    std::string useLicenseUrl;             // Use License URL
    std::string validationUrl;             // Validation URL
    std::string partialProductKey;         // Last 5 chars
    int remainingRearmCount;               // Remaining Windows rearm count
    std::string trustedTime;               // Trusted time
    std::map<std::string, std::string> additionalInfo;  // Other fields
};

// Detailed Office License Information
struct OfficeLicenseInfo {
    std::string productId;                 // Product ID
    std::string skuId;                     // SKU ID
    std::string licenseName;               // License Name
    std::string licenseDescription;        // License Description
    std::string licenseStatus;             // License Status (LICENSED, GRACE, etc)
    std::string remainingGrace;            // Remaining grace period
    std::string partialProductKey;         // Last 5 characters
    std::string activationType;            // Activation Type
    std::string dnsAutoDiscovery;          // DNS auto-discovery
    std::string kmsServer;                 // KMS machine registry override
    std::string activationInterval;        // Activation Interval
    std::string renewalInterval;           // Renewal Interval
    std::string kmsHostCaching;            // KMS host caching
    std::map<std::string, std::string> additionalInfo;  // Other fields
};
