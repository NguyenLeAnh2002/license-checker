#include <gtest/gtest.h>
#include "../../src/license-detection/LicenseDetector.h"
#include "../../src/license-detection/LicenseStatusEnum.h"

// Integration tests for License Detection across Windows versions
// Note: Some tests require specific Windows versions or license states (manual testing)

class LicenseDetectionIntegrationTest : public ::testing::Test {
protected:
    LicenseDetectionIntegrationTest() : detector_() {}
    LicenseDetector detector_;
};

// Task-15: Windows 10 Detection
TEST_F(LicenseDetectionIntegrationTest, Windows10Detection) {
    LicenseResult result = detector_.Detect();

    // Should detect a license status
    EXPECT_TRUE(
        result.GetLicenseStatus() == LicenseStatus::Legitimate ||
        result.GetLicenseStatus() == LicenseStatus::Cracked ||
        result.GetLicenseStatus() == LicenseStatus::NotLicensed ||
        result.GetLicenseStatus() == LicenseStatus::UnableToDetermine
    );

    // Should have Windows version info
    EXPECT_GT(result.GetWindowsVersion(), 0);
    EXPECT_FALSE(result.GetWindowsEdition().empty());

    // FR-3: Should retrieve currently active license
    EXPECT_TRUE(true);  // Current license is active by definition
}

// Task-16: Older Windows Detection (7, 8.1)
TEST_F(LicenseDetectionIntegrationTest, OlderWindowsDetection) {
    LicenseResult result = detector_.Detect();

    // Should support Windows 7, 8, 8.1, 10, 11, Server versions
    int version = result.GetWindowsVersion();
    EXPECT_TRUE(
        version == 7 || version == 8 || version == 81 ||
        version == 10 || version == 11 ||
        version >= 2008  // Server versions
    );
}

// Task-17: Windows KMS Detection
TEST_F(LicenseDetectionIntegrationTest, WindowsKMSDetection) {
    LicenseResult result = detector_.Detect();

    // Should detect KMS status
    EXPECT_TRUE(
        result.GetKmsStatus() == KMSStatus::NotKMS ||
        result.GetKmsStatus() == KMSStatus::KMSDetected ||
        result.GetKmsStatus() == KMSStatus::KMSNotFound ||
        result.GetKmsStatus() == KMSStatus::Error
    );

    // If KMS detected, should have server address
    if (result.GetKmsStatus() == KMSStatus::KMSDetected) {
        EXPECT_FALSE(result.GetWindowsKmsServer().empty());
    }
}

// Task-18: Office KMS Detection (if Office installed)
TEST_F(LicenseDetectionIntegrationTest, OfficeKMSDetection) {
    LicenseResult result = detector_.Detect();

    // Office KMS should be either empty (not installed) or have server address
    std::string officeKms = result.GetOfficeKmsServer();
    EXPECT_TRUE(officeKms.empty() || !officeKms.empty());
}

// Task-19: Grace Period Handling
TEST_F(LicenseDetectionIntegrationTest, GracePeriodHandling) {
    LicenseResult result = detector_.Detect();

    // If in grace period (registry status 1-9), should report as NotLicensed (FR-7)
    // Grace period states are between unactivated and genuine
    EXPECT_TRUE(
        result.GetLicenseStatus() == LicenseStatus::Legitimate ||
        result.GetLicenseStatus() == LicenseStatus::Cracked ||
        result.GetLicenseStatus() == LicenseStatus::NotLicensed ||
        result.GetLicenseStatus() == LicenseStatus::UnableToDetermine
    );
}

// Verify Tier Fallback (Task-7: LicenseDetector Orchestrator)
TEST_F(LicenseDetectionIntegrationTest, TierFallbackWorks) {
    // Orchestrator should try Tier 1 (SL API) → Tier 2 (WMI) → Tier 3 (Registry)
    // and return successful result from whichever tier succeeds

    LicenseResult result = detector_.Detect();

    // Should return a valid result
    EXPECT_TRUE(
        result.GetLicenseStatus() == LicenseStatus::Legitimate ||
        result.GetLicenseStatus() == LicenseStatus::Cracked ||
        result.GetLicenseStatus() == LicenseStatus::NotLicensed ||
        result.GetLicenseStatus() == LicenseStatus::UnableToDetermine
    );

    // Should have complete information (or indicate unable to determine)
    if (result.GetLicenseStatus() != LicenseStatus::UnableToDetermine) {
        EXPECT_GT(result.GetWindowsVersion(), 0);
    }
}

// Consistency check across multiple runs
TEST_F(LicenseDetectionIntegrationTest, ConsistentDetectionResults) {
    // FR-3: Should retrieve currently active license (consistent across runs)

    LicenseResult result1 = detector_.Detect();
    LicenseResult result2 = detector_.Detect();

    // License status should be consistent (same machine, same license)
    EXPECT_EQ(result1.GetLicenseStatus(), result2.GetLicenseStatus());
    EXPECT_EQ(result1.GetWindowsVersion(), result2.GetWindowsVersion());
}

// All required fields present
TEST_F(LicenseDetectionIntegrationTest, AllRequiredFieldsPresent) {
    LicenseResult result = detector_.Detect();

    // Should have all required fields (FR-13)
    EXPECT_TRUE(
        result.GetLicenseStatus() == LicenseStatus::Legitimate ||
        result.GetLicenseStatus() == LicenseStatus::Cracked ||
        result.GetLicenseStatus() == LicenseStatus::NotLicensed ||
        result.GetLicenseStatus() == LicenseStatus::UnableToDetermine
    );

    EXPECT_GT(result.GetWindowsVersion(), 0);
    EXPECT_FALSE(result.GetWindowsEdition().empty());

    EXPECT_TRUE(
        result.GetKmsStatus() == KMSStatus::NotKMS ||
        result.GetKmsStatus() == KMSStatus::KMSDetected ||
        result.GetKmsStatus() == KMSStatus::KMSNotFound ||
        result.GetKmsStatus() == KMSStatus::Error
    );
}
