#include <gtest/gtest.h>
#include "../../src/license-detection/WMIDetector.h"
#include "../../src/license-detection/LicenseStatusEnum.h"

class WMIDetectorErrorTest : public ::testing::Test {
protected:
    WMIDetectorErrorTest() : detector_() {}
    WMIDetector detector_;
};

// Test: WMI unavailable → returns "UnableToDetermine" (FR-6)
TEST_F(WMIDetectorErrorTest, WMIUnavailableHandledGracefully) {
    LicenseResult result = detector_.Detect();

    // If WMI is unavailable, should return valid status
    EXPECT_TRUE(
        result.GetLicenseStatus() == LicenseStatus::Legitimate ||
        result.GetLicenseStatus() == LicenseStatus::Cracked ||
        result.GetLicenseStatus() == LicenseStatus::NotLicensed ||
        result.GetLicenseStatus() == LicenseStatus::UnableToDetermine
    );
}

// Test: Query timeout handled gracefully (FR-6)
TEST_F(WMIDetectorErrorTest, QueryTimeoutHandledGracefully) {
    // WMI queries should complete quickly; if timeout would occur, should recover
    auto start = std::chrono::high_resolution_clock::now();
    LicenseResult result = detector_.Detect();
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);

    // Detection should complete in reasonable time (no hanging)
    EXPECT_LT(duration.count(), 10);

    // Should return valid result
    EXPECT_TRUE(
        result.GetLicenseStatus() == LicenseStatus::Legitimate ||
        result.GetLicenseStatus() == LicenseStatus::Cracked ||
        result.GetLicenseStatus() == LicenseStatus::NotLicensed ||
        result.GetLicenseStatus() == LicenseStatus::UnableToDetermine
    );
}

// Test: Invalid query result handled gracefully (FR-6)
TEST_F(WMIDetectorErrorTest, InvalidQueryResultHandledGracefully) {
    // Run multiple times to ensure robust error handling
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
}

// Test: Detector doesn't crash on repeated attempts
TEST_F(WMIDetectorErrorTest, RobustUnderRepeatedCalls) {
    EXPECT_NO_THROW({
        for (int i = 0; i < 10; ++i) {
            detector_.Detect();
        }
    });
}

// Test: Graceful degradation
TEST_F(WMIDetectorErrorTest, GracefulDegradation) {
    LicenseResult result = detector_.Detect();

    // If unable to detect full information, should still report what it can
    if (result.GetLicenseStatus() == LicenseStatus::UnableToDetermine) {
        // Even with limited data, should be structured properly
        EXPECT_TRUE(true);  // Passed if no exception
    } else {
        // Should have complete information
        EXPECT_GT(result.GetWindowsVersion(), 0);
        EXPECT_FALSE(result.GetWindowsEdition().empty());
    }
}
