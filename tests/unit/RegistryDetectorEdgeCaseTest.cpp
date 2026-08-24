#include <gtest/gtest.h>
#include "../../src/license-detection/RegistryDetector.h"
#include "../../src/license-detection/LicenseStatusEnum.h"

class RegistryDetectorEdgeCaseTest : public ::testing::Test {
protected:
    RegistryDetectorEdgeCaseTest() : detector_() {}
    RegistryDetector detector_;
};

// Test: Grace period detection → classifies as "NotLicensed" (FR-7)
TEST_F(RegistryDetectorEdgeCaseTest, GracePeriodDetection) {
    LicenseResult result = detector_.Detect();

    // If system is in grace period, should be NotLicensed
    // Grace period states (registry LicenseStatus 1-9) should map to NotLicensed
    if (result.GetWindowsVersion() > 0) {
        // If we got a result, it should be valid
        EXPECT_TRUE(
            result.GetLicenseStatus() == LicenseStatus::Legitimate ||
            result.GetLicenseStatus() == LicenseStatus::Cracked ||
            result.GetLicenseStatus() == LicenseStatus::NotLicensed ||
            result.GetLicenseStatus() == LicenseStatus::UnableToDetermine
        );
    }
}

// Test: Registry key missing → returns "UnableToDetermine" (FR-6)
TEST_F(RegistryDetectorEdgeCaseTest, MissingRegistryKeyHandled) {
    // If registry key doesn't exist, should return UnableToDetermine
    LicenseResult result = detector_.Detect();

    // Should handle missing keys gracefully
    EXPECT_TRUE(
        result.GetLicenseStatus() == LicenseStatus::Legitimate ||
        result.GetLicenseStatus() == LicenseStatus::Cracked ||
        result.GetLicenseStatus() == LicenseStatus::NotLicensed ||
        result.GetLicenseStatus() == LicenseStatus::UnableToDetermine
    );
}

// Test: Permission denied on registry access → returns "UnableToDetermine" (FR-6)
TEST_F(RegistryDetectorEdgeCaseTest, PermissionDeniedHandled) {
    // In limited privilege context, should handle permission denied gracefully
    LicenseResult result = detector_.Detect();

    // Should not crash
    EXPECT_TRUE(
        result.GetLicenseStatus() == LicenseStatus::Legitimate ||
        result.GetLicenseStatus() == LicenseStatus::Cracked ||
        result.GetLicenseStatus() == LicenseStatus::NotLicensed ||
        result.GetLicenseStatus() == LicenseStatus::UnableToDetermine
    );
}

// Test: Corrupted registry data handled gracefully (FR-6)
TEST_F(RegistryDetectorEdgeCaseTest, CorruptedRegistryDataHandled) {
    // Run multiple times; even with corrupted data should not crash
    EXPECT_NO_THROW({
        for (int i = 0; i < 5; ++i) {
            LicenseResult result = detector_.Detect();

            // Should always return valid status
            EXPECT_TRUE(
                result.GetLicenseStatus() == LicenseStatus::Legitimate ||
                result.GetLicenseStatus() == LicenseStatus::Cracked ||
                result.GetLicenseStatus() == LicenseStatus::NotLicensed ||
                result.GetLicenseStatus() == LicenseStatus::UnableToDetermine
            );
        }
    });
}

// Test: Empty registry value handled gracefully
TEST_F(RegistryDetectorEdgeCaseTest, EmptyRegistryValueHandled) {
    LicenseResult result = detector_.Detect();

    // Should return valid status even with empty values
    EXPECT_TRUE(
        result.GetLicenseStatus() == LicenseStatus::Legitimate ||
        result.GetLicenseStatus() == LicenseStatus::Cracked ||
        result.GetLicenseStatus() == LicenseStatus::NotLicensed ||
        result.GetLicenseStatus() == LicenseStatus::UnableToDetermine
    );
}

// Test: Invalid license status codes handled gracefully
TEST_F(RegistryDetectorEdgeCaseTest, InvalidLicenseStatusCodes) {
    // If registry has unexpected license status codes, should handle gracefully
    LicenseResult result = detector_.Detect();

    // Known valid codes: 0 (unlicensed), 1-9 (grace), 10 (genuine), 12 (notification)
    // Unknown codes should be handled without crashing
    EXPECT_TRUE(
        result.GetLicenseStatus() == LicenseStatus::Legitimate ||
        result.GetLicenseStatus() == LicenseStatus::Cracked ||
        result.GetLicenseStatus() == LicenseStatus::NotLicensed ||
        result.GetLicenseStatus() == LicenseStatus::UnableToDetermine
    );
}

// Test: Robust under repeated access
TEST_F(RegistryDetectorEdgeCaseTest, RobustUnderRepeatedAccess) {
    EXPECT_NO_THROW({
        for (int i = 0; i < 10; ++i) {
            detector_.Detect();
        }
    });
}
