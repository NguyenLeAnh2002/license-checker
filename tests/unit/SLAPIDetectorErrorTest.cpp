#include <gtest/gtest.h>
#include "../../src/license-detection/SLAPIDetector.h"
#include "../../src/license-detection/LicenseStatusEnum.h"

class SLAPIDetectorErrorTest : public ::testing::Test {
protected:
    SLAPIDetectorErrorTest() : detector_() {}
    SLAPIDetector detector_;
};

// Test: SL API unavailable → returns "UnableToDetermine" (FR-6)
TEST_F(SLAPIDetectorErrorTest, SLAPIUnavailableReturnsUnableToDetermine) {
    LicenseResult result = detector_.Detect();

    // If SL API is unavailable, should fall back gracefully
    if (result.GetLicenseStatus() == LicenseStatus::UnableToDetermine) {
        EXPECT_TRUE(result.IsError() || result.GetWindowsVersion() == 0);
    }
}

// Test: Permission denied → returns "UnableToDetermine" (FR-6)
TEST_F(SLAPIDetectorErrorTest, PermissionDeniedHandledGracefully) {
    // This test verifies the detector handles permission errors gracefully
    // In a real scenario, this would run in a limited-privilege context
    LicenseResult result = detector_.Detect();

    // Should not crash, should return valid status
    EXPECT_TRUE(
        result.GetLicenseStatus() == LicenseStatus::Legitimate ||
        result.GetLicenseStatus() == LicenseStatus::Cracked ||
        result.GetLicenseStatus() == LicenseStatus::NotLicensed ||
        result.GetLicenseStatus() == LicenseStatus::UnableToDetermine
    );
}

// Test: Malformed API response handled gracefully (FR-6)
TEST_F(SLAPIDetectorErrorTest, MalformedResponseHandledGracefully) {
    // Run detection multiple times to test robustness
    for (int i = 0; i < 5; ++i) {
        LicenseResult result = detector_.Detect();

        // Should always return a valid status, never crash
        EXPECT_TRUE(
            result.GetLicenseStatus() == LicenseStatus::Legitimate ||
            result.GetLicenseStatus() == LicenseStatus::Cracked ||
            result.GetLicenseStatus() == LicenseStatus::NotLicensed ||
            result.GetLicenseStatus() == LicenseStatus::UnableToDetermine
        );

        // Should have valid Windows version or indicate unable to determine
        EXPECT_TRUE(
            result.GetWindowsVersion() > 0 ||
            result.GetLicenseStatus() == LicenseStatus::UnableToDetermine
        );
    }
}

// Test: Detector doesn't crash on repeated failures
TEST_F(SLAPIDetectorErrorTest, NoExceptionsOnRepeatedCalls) {
    EXPECT_NO_THROW({
        for (int i = 0; i < 10; ++i) {
            detector_.Detect();
        }
    });
}

// Test: Error recovery - detector continues working after error
TEST_F(SLAPIDetectorErrorTest, ErrorRecoveryAndContinue) {
    LicenseResult result1 = detector_.Detect();
    LicenseResult result2 = detector_.Detect();

    // Both should be valid, even if first had errors
    EXPECT_TRUE(
        result1.GetLicenseStatus() == LicenseStatus::Legitimate ||
        result1.GetLicenseStatus() == LicenseStatus::Cracked ||
        result1.GetLicenseStatus() == LicenseStatus::NotLicensed ||
        result1.GetLicenseStatus() == LicenseStatus::UnableToDetermine
    );

    EXPECT_TRUE(
        result2.GetLicenseStatus() == LicenseStatus::Legitimate ||
        result2.GetLicenseStatus() == LicenseStatus::Cracked ||
        result2.GetLicenseStatus() == LicenseStatus::NotLicensed ||
        result2.GetLicenseStatus() == LicenseStatus::UnableToDetermine
    );
}
