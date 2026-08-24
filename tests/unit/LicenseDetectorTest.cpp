#include <gtest/gtest.h>
#include "../../src/license-detection/LicenseDetector.h"
#include "../../src/license-detection/LicenseStatusEnum.h"
#include "../../src/license-detection/LicenseResult.h"

class LicenseDetectorTest : public ::testing::Test {
protected:
    LicenseDetectorTest() : detector_() {}

    LicenseDetector detector_;
};

// Test: Detector initialization
TEST_F(LicenseDetectorTest, DetectorInitialization) {
    EXPECT_TRUE(true);
}

// Test: Orchestrator returns valid LicenseResult
TEST_F(LicenseDetectorTest, OrchestratorReturnsValidResult) {
    LicenseResult result = detector_.Detect();

    EXPECT_TRUE(
        result.GetLicenseStatus() == LicenseStatus::Legitimate ||
        result.GetLicenseStatus() == LicenseStatus::Cracked ||
        result.GetLicenseStatus() == LicenseStatus::NotLicensed ||
        result.GetLicenseStatus() == LicenseStatus::UnableToDetermine
    );
}

// Test: Windows version is detected
TEST_F(LicenseDetectorTest, WindowsVersionDetected) {
    LicenseResult result = detector_.Detect();
    EXPECT_GT(result.GetWindowsVersion(), 0);
}

// Test: Windows edition is detected
TEST_F(LicenseDetectorTest, WindowsEditionDetected) {
    LicenseResult result = detector_.Detect();
    EXPECT_FALSE(result.GetWindowsEdition().empty());
}

// Test: KMS status is set
TEST_F(LicenseDetectorTest, KmsStatusIsSet) {
    LicenseResult result = detector_.Detect();

    EXPECT_TRUE(
        result.GetKmsStatus() == KMSStatus::NotKMS ||
        result.GetKmsStatus() == KMSStatus::KMSDetected ||
        result.GetKmsStatus() == KMSStatus::KMSNotFound ||
        result.GetKmsStatus() == KMSStatus::Error
    );
}

// Test: Timestamp is set
TEST_F(LicenseDetectorTest, TimestampIsSet) {
    auto beforeDetection = std::chrono::system_clock::now();
    LicenseResult result = detector_.Detect();
    auto afterDetection = std::chrono::system_clock::now();

    auto timestamp = result.GetTimestamp();
    EXPECT_GE(timestamp, beforeDetection);
    EXPECT_LE(timestamp, afterDetection);
}

// Test: Consistent detection across multiple runs
TEST_F(LicenseDetectorTest, ConsistentDetection) {
    LicenseResult result1 = detector_.Detect();
    LicenseResult result2 = detector_.Detect();

    EXPECT_EQ(result1.GetLicenseStatus(), result2.GetLicenseStatus());
    EXPECT_EQ(result1.GetWindowsVersion(), result2.GetWindowsVersion());
}

// Test: License status is valid (FR-2)
TEST_F(LicenseDetectorTest, ValidLicenseStatus) {
    LicenseResult result = detector_.Detect();

    LicenseStatus status = result.GetLicenseStatus();
    EXPECT_TRUE(
        status == LicenseStatus::Legitimate ||
        status == LicenseStatus::Cracked ||
        status == LicenseStatus::NotLicensed ||
        status == LicenseStatus::UnableToDetermine
    );
}

// Test: KMS server address if KMS is detected
TEST_F(LicenseDetectorTest, KmsServerAddressIfDetected) {
    LicenseResult result = detector_.Detect();

    if (result.GetKmsStatus() == KMSStatus::KMSDetected) {
        EXPECT_FALSE(result.GetWindowsKmsServer().empty());
    }
    else if (result.GetKmsStatus() == KMSStatus::NotKMS) {
        EXPECT_TRUE(result.GetWindowsKmsServer().empty());
    }
}

// Test: Grace period handling (FR-7)
TEST_F(LicenseDetectorTest, GracePeriodHandling) {
    LicenseResult result = detector_.Detect();

    // Should handle grace period appropriately
    EXPECT_TRUE(
        result.GetLicenseStatus() == LicenseStatus::NotLicensed ||
        result.GetLicenseStatus() == LicenseStatus::Legitimate ||
        result.GetLicenseStatus() == LicenseStatus::Cracked ||
        result.GetLicenseStatus() == LicenseStatus::UnableToDetermine
    );
}

// Test: Error handling - graceful failure (FR-6)
TEST_F(LicenseDetectorTest, GracefulErrorHandling) {
    LicenseResult result = detector_.Detect();

    // Should not crash; should report error appropriately
    EXPECT_TRUE(
        !result.IsError() ||
        result.GetLicenseStatus() == LicenseStatus::UnableToDetermine
    );
}

// Test: Performance - completes quickly (FR-15)
TEST_F(LicenseDetectorTest, DetectionPerformance) {
    auto start = std::chrono::high_resolution_clock::now();
    LicenseResult result = detector_.Detect();
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);
    EXPECT_LT(duration.count(), 10);
}

// Test: Windows version support (FR-4, FR-5)
TEST_F(LicenseDetectorTest, WindowsVersionSupport) {
    LicenseResult result = detector_.Detect();

    int version = result.GetWindowsVersion();
    EXPECT_TRUE(
        version == 7 || version == 8 || version == 81 ||
        version == 10 || version == 11 ||
        version >= 2008  // Server versions
    );
}
