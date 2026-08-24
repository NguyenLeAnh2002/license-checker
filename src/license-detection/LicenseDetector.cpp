#include "LicenseDetector.h"
#include <iostream>
#include <iomanip>

LicenseDetector::LicenseDetector()
    : slMgrDetector_(std::make_unique<WindowsSLMgrDetector>()),
      slApiDetector_(std::make_unique<SLAPIDetector>()),
      wmiDetector_(std::make_unique<WMIDetector>()),
      registryDetector_(std::make_unique<RegistryDetector>()),
      osppDetector_(std::make_unique<OfficeOSPPDetector>()) {
}

LicenseDetector::~LicenseDetector() {
}

// Main orchestration method: multi-tier fallback strategy
LicenseResult LicenseDetector::Detect() {
    std::cout << "[DEBUG] ===== License Detection Started =====" << std::endl;
    LicenseResult result;
    // Tier 1: Try Windows SLMgr detection (most direct method)
    //std::cout << "[DEBUG] Tier 1: Attempting Windows SLMgr detection..." << std::endl;
    //LicenseResult result = slMgrDetector_->Detect();
    //if (IsSuccessfulDetection(result)) {
    //    std::cout << "[DEBUG] Tier 1 SUCCESS: " << result.GetWindowsEdition() << std::endl;
    //    // Also try to detect Office license
    //    OfficeLicenseInfo officeInfo = osppDetector_->Detect();
    //    result.SetOfficeLicenseInfo(officeInfo);
    //    result.SetOfficeKmsServer(officeInfo.kmsServer);
    //    return result;  // Success on Tier 1
    //}
    //std::cout << "[DEBUG] Tier 1 FAILED: " << (result.IsError() ? result.GetErrorMessage() : "Status=UnableToDetermine") << std::endl;

    // Tier 2: Try SL API detection (fastest, most accurate on Windows 10/11)
    //std::cout << "[DEBUG] Tier 2: Attempting SL API detection..." << std::endl;
    //result = slApiDetector_->Detect();
    //if (IsSuccessfulDetection(result)) {
    //    std::cout << "[DEBUG] Tier 2 SUCCESS: " << result.GetWindowsEdition() << std::endl;
    //    // Also try to detect Office license
    //    OfficeLicenseInfo officeInfo = osppDetector_->Detect();
    //    result.SetOfficeLicenseInfo(officeInfo);
    //    result.SetOfficeKmsServer(officeInfo.kmsServer);
    //    return result;  // Success on Tier 2
    //}
    //std::cout << "[DEBUG] Tier 2 FAILED: " << (result.IsError() ? result.GetErrorMessage() : "Status=UnableToDetermine") << std::endl;

    // Tier 3: Try WMI detection (fallback for older Windows)
    //std::cout << "[DEBUG] Tier 3: Attempting WMI detection..." << std::endl;
    //result = wmiDetector_->Detect();
    //if (IsSuccessfulDetection(result)) {
    //    std::cout << "[DEBUG] Tier 3 SUCCESS: " << result.GetWindowsEdition() << std::endl;
    //    // Also try to detect Office license
    //    OfficeLicenseInfo officeInfo = osppDetector_->Detect();
    //    result.SetOfficeLicenseInfo(officeInfo);
    //    result.SetOfficeKmsServer(officeInfo.kmsServer);
    //    return result;  // Success on Tier 3
    //}
    //std::cout << "[DEBUG] Tier 3 FAILED: " << (result.IsError() ? result.GetErrorMessage() : "Status=UnableToDetermine") << std::endl;

    // Tier 4: Try registry detection (last resort)
    std::cout << "[DEBUG] Tier 4: Attempting Registry detection..." << std::endl;
    result = registryDetector_->Detect();
    if (IsSuccessfulDetection(result)) {
        std::cout << "[DEBUG] ✓ Tier 4 SUCCESS: " << result.GetWindowsEdition() << std::endl;
        // Also try to detect Office license
        OfficeLicenseInfo officeInfo = osppDetector_->Detect();
        result.SetOfficeLicenseInfo(officeInfo);
        result.SetOfficeKmsServer(officeInfo.kmsServer);
        return result;  // Success on Tier 4
    }
    std::cout << "[DEBUG] Tier 4 FAILED: " << (result.IsError() ? result.GetErrorMessage() : "Status=UnableToDetermine") << std::endl;

    // All tiers failed - return UnableToDetermine
    std::cout << "[DEBUG] ALL TIERS FAILED " << std::endl;
    if (result.GetLicenseStatus() == LicenseStatus::UnableToDetermine) {
        result.SetError(true);
        if (result.GetErrorMessage().empty()) {
            result.SetErrorMessage("All detection tiers failed: SLMgr, SL API, WMI, and Registry");
        }
    }

    return result;
}

// Helper: Determine if a detection result is successful
bool LicenseDetector::IsSuccessfulDetection(const LicenseResult& result) {
    // A detection is successful if:
    // 1. Status is not "UnableToDetermine"
    // 2. No error flag is set
    // 3. Windows version is valid (> 0)

    return result.GetLicenseStatus() != LicenseStatus::UnableToDetermine &&
           !result.IsError() &&
           result.GetWindowsVersion() > 0;
}
