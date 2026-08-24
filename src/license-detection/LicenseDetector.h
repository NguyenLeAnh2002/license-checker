#pragma once

#include "LicenseResult.h"
#include "SLAPIDetector.h"
#include "WMIDetector.h"
#include "RegistryDetector.h"
#include "WindowsSLMgrDetector.h"
#include "OfficeOSPPDetector.h"
#include <memory>

// Main license detection orchestrator
// Implements multi-tier fallback strategy for Windows and Office detection
class LicenseDetector {
public:
    LicenseDetector();
    ~LicenseDetector();

    // Delete copy semantics
    LicenseDetector(const LicenseDetector&) = delete;
    LicenseDetector& operator=(const LicenseDetector&) = delete;

    // Allow move semantics
    LicenseDetector(LicenseDetector&&) noexcept = default;
    LicenseDetector& operator=(LicenseDetector&&) noexcept = default;

    // Main detection method: detects Windows and Office licenses
    LicenseResult Detect();

private:
    // Detector instances
    std::unique_ptr<WindowsSLMgrDetector> slMgrDetector_;
    std::unique_ptr<SLAPIDetector> slApiDetector_;
    std::unique_ptr<WMIDetector> wmiDetector_;
    std::unique_ptr<RegistryDetector> registryDetector_;
    std::unique_ptr<OfficeOSPPDetector> osppDetector_;

    // Helper: Check if detection result is successful
    static bool IsSuccessfulDetection(const LicenseResult& result);
};
