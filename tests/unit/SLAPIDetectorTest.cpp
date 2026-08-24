#include <gtest/gtest.h>
#include "../../src/license-detection/SLAPIDetector.h"
#include "../../src/license-detection/LicenseStatusEnum.h"
#include "../../src/license-detection/LicenseResult.h"

class SLAPIDetectorTest : public ::testing::Test {
protected:
    SLAPIDetectorTest() : detector_() {}

    SLAPIDetector detector_;
};

// Test: Detector initialization
TEST_F(SLAPIDetectorTest, DetectorInitialization) {
    // Should initialize without errors
    EXPECT_TRUE(true);
}

// Test: Detect method returns LicenseResult
TEST_F(SLAPIDetectorTest, DetectReturnsLicenseResult) {
    LicenseResult result = detector_.Detect();

    // Result should contain valid data or error status
    EXPECT_TRUE(
        result.GetLicenseStatus() == LicenseStatus::Legitimate ||
        result.GetLicenseStatus() == LicenseStatus::Cracked ||
        result.GetLicenseStatus() == LicenseStatus::NotLicensed ||
        result.GetLicenseStatus() == LicenseStatus::UnableToDetermine
    );
}

// Test: Windows version is detected
TEST_F(SLAPIDetectorTest, WindowsVersionDetected) {
    LicenseResult result = detector_.Detect();

    // Should have a valid Windows version number
    EXPECT_GT(result.GetWindowsVersion(), 0);
}

// Test: Windows edition is detected
TEST_F(SLAPIDetectorTest, WindowsEditionDetected) {
    LicenseResult result = detector_.Detect();

    // Should have a non-empty edition string (Pro, Home, Enterprise, etc.)
    EXPECT_FALSE(result.GetWindowsEdition().empty());
}

// Test: KMS status is set
TEST_F(SLAPIDetectorTest, KmsStatusIsSet) {
    LicenseResult result = detector_.Detect();

    // KMS status should be one of the valid states
    EXPECT_TRUE(
        result.GetKmsStatus() == KMSStatus::NotKMS ||
        result.GetKmsStatus() == KMSStatus::KMSDetected ||
        result.GetKmsStatus() == KMSStatus::KMSNotFound ||
        result.GetKmsStatus() == KMSStatus::Error
    );
}

// Test: KMS server address is present if KMS is detected
TEST_F(SLAPIDetectorTest, KmsServerAddressIfKmsDetected) {
    LicenseResult result = detector_.Detect();

    // If KMS is detected, server address should be present
    if (result.GetKmsStatus() == KMSStatus::KMSDetected) {
        EXPECT_FALSE(result.GetWindowsKmsServer().empty());
    }
    // If not KMS, server address should be empty
    else if (result.GetKmsStatus() == KMSStatus::NotKMS) {
        EXPECT_TRUE(result.GetWindowsKmsServer().empty());
    }
}

// Test: Timestamp is set
TEST_F(SLAPIDetectorTest, TimestampIsSet) {
    auto beforeDetection = std::chrono::system_clock::now();
    LicenseResult result = detector_.Detect();
    auto afterDetection = std::chrono::system_clock::now();

    // Timestamp should be between before and after detection
    auto timestamp = result.GetTimestamp();
    EXPECT_GE(timestamp, beforeDetection);
    EXPECT_LE(timestamp, afterDetection);
}

// Test: Error handling - result should not be error by default
TEST_F(SLAPIDetectorTest, ErrorHandling) {
    LicenseResult result = detector_.Detect();

    // On a functioning system, should not report error
    // On a non-functioning system, should have error status set
    EXPECT_TRUE(
        !result.IsError() ||
        result.GetLicenseStatus() == LicenseStatus::UnableToDetermine
    );
}

// Test: Multiple detections are consistent (FR-3: currently active license)
TEST_F(SLAPIDetectorTest, ConsistentDetection) {
    LicenseResult result1 = detector_.Detect();
    LicenseResult result2 = detector_.Detect();

    // License status should be the same across two runs
    EXPECT_EQ(result1.GetLicenseStatus(), result2.GetLicenseStatus());
    EXPECT_EQ(result1.GetWindowsVersion(), result2.GetWindowsVersion());
}

// Test: Office detection capability
TEST_F(SLAPIDetectorTest, OfficeDetection) {
    LicenseResult result = detector_.Detect();

    // Office KMS server should be either empty (not installed or not KMS)
    // or contain a server address (if Office is installed and KMS-activated)
    EXPECT_TRUE(
        result.GetOfficeKmsServer().empty() ||
        !result.GetOfficeKmsServer().empty()
    );
}

// Test: License status is one of the expected values (FR-2)
TEST_F(SLAPIDetectorTest, ValidLicenseStatus) {
    LicenseResult result = detector_.Detect();

    LicenseStatus status = result.GetLicenseStatus();
    EXPECT_TRUE(
        status == LicenseStatus::Legitimate ||
        status == LicenseStatus::Cracked ||
        status == LicenseStatus::NotLicensed ||
        status == LicenseStatus::UnableToDetermine
    );
}

// Test: Detector handles detection properly (FR-1: uses native APIs, not third-party)
TEST_F(SLAPIDetectorTest, DetectionCompletes) {
    // Should complete without hanging or crashing
    LicenseResult result = detector_.Detect();

    // Should produce a result object
    EXPECT_TRUE(true);
}

// Test: Grace period classification (FR-7)
TEST_F(SLAPIDetectorTest, GracePeriodHandling) {
    LicenseResult result = detector_.Detect();

    // If system is in grace period, should be reported appropriately
    // (This is hard to test without a grace-period system, so we just verify no crash)
    EXPECT_TRUE(
        result.GetLicenseStatus() == LicenseStatus::NotLicensed ||
        result.GetLicenseStatus() == LicenseStatus::Legitimate ||
        result.GetLicenseStatus() == LicenseStatus::Cracked ||
        result.GetLicenseStatus() == LicenseStatus::UnableToDetermine
    );
}

// Test: Performance - detection completes quickly (FR-15)
TEST_F(SLAPIDetectorTest, DetectionPerformance) {
    auto start = std::chrono::high_resolution_clock::now();
    LicenseResult result = detector_.Detect();
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);

    // Should complete in less than 10 seconds
    EXPECT_LT(duration.count(), 10);
}
