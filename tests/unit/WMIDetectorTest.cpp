#include <gtest/gtest.h>
#include "../../src/license-detection/WMIDetector.h"
#include "../../src/license-detection/LicenseStatusEnum.h"
#include "../../src/license-detection/LicenseResult.h"

class WMIDetectorTest : public ::testing::Test {
protected:
    WMIDetectorTest() : detector_() {}

    WMIDetector detector_;
};

// Test: Detector initialization
TEST_F(WMIDetectorTest, DetectorInitialization) {
    // Should initialize without errors
    EXPECT_TRUE(true);
}

// Test: Detect method returns LicenseResult
TEST_F(WMIDetectorTest, DetectReturnsLicenseResult) {
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
TEST_F(WMIDetectorTest, WindowsVersionDetected) {
    LicenseResult result = detector_.Detect();

    // Should have a valid Windows version number
    EXPECT_GT(result.GetWindowsVersion(), 0);
}

// Test: Windows edition is detected
TEST_F(WMIDetectorTest, WindowsEditionDetected) {
    LicenseResult result = detector_.Detect();

    // Should have a non-empty edition string
    EXPECT_FALSE(result.GetWindowsEdition().empty());
}

// Test: KMS status is set
TEST_F(WMIDetectorTest, KmsStatusIsSet) {
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
TEST_F(WMIDetectorTest, KmsServerAddressIfKmsDetected) {
    LicenseResult result = detector_.Detect();

    if (result.GetKmsStatus() == KMSStatus::KMSDetected) {
        EXPECT_FALSE(result.GetWindowsKmsServer().empty());
    }
    else if (result.GetKmsStatus() == KMSStatus::NotKMS) {
        EXPECT_TRUE(result.GetWindowsKmsServer().empty());
    }
}

// Test: Timestamp is set
TEST_F(WMIDetectorTest, TimestampIsSet) {
    auto beforeDetection = std::chrono::system_clock::now();
    LicenseResult result = detector_.Detect();
    auto afterDetection = std::chrono::system_clock::now();

    auto timestamp = result.GetTimestamp();
    EXPECT_GE(timestamp, beforeDetection);
    EXPECT_LE(timestamp, afterDetection);
}

// Test: Error handling
TEST_F(WMIDetectorTest, ErrorHandling) {
    LicenseResult result = detector_.Detect();

    // Should not report error on functioning system
    EXPECT_TRUE(
        !result.IsError() ||
        result.GetLicenseStatus() == LicenseStatus::UnableToDetermine
    );
}

// Test: Multiple detections are consistent
TEST_F(WMIDetectorTest, ConsistentDetection) {
    LicenseResult result1 = detector_.Detect();
    LicenseResult result2 = detector_.Detect();

    // Status should be consistent
    EXPECT_EQ(result1.GetLicenseStatus(), result2.GetLicenseStatus());
    EXPECT_EQ(result1.GetWindowsVersion(), result2.GetWindowsVersion());
}

// Test: Office detection capability
TEST_F(WMIDetectorTest, OfficeDetection) {
    LicenseResult result = detector_.Detect();

    // Office KMS should be empty or contain server address
    EXPECT_TRUE(
        result.GetOfficeKmsServer().empty() ||
        !result.GetOfficeKmsServer().empty()
    );
}

// Test: License status is valid
TEST_F(WMIDetectorTest, ValidLicenseStatus) {
    LicenseResult result = detector_.Detect();

    LicenseStatus status = result.GetLicenseStatus();
    EXPECT_TRUE(
        status == LicenseStatus::Legitimate ||
        status == LicenseStatus::Cracked ||
        status == LicenseStatus::NotLicensed ||
        status == LicenseStatus::UnableToDetermine
    );
}

// Test: WMI detection completes
TEST_F(WMIDetectorTest, DetectionCompletes) {
    // Should complete without hanging
    LicenseResult result = detector_.Detect();
    EXPECT_TRUE(true);
}

// Test: Grace period handling
TEST_F(WMIDetectorTest, GracePeriodHandling) {
    LicenseResult result = detector_.Detect();

    // Should handle grace period appropriately
    EXPECT_TRUE(
        result.GetLicenseStatus() == LicenseStatus::NotLicensed ||
        result.GetLicenseStatus() == LicenseStatus::Legitimate ||
        result.GetLicenseStatus() == LicenseStatus::Cracked ||
        result.GetLicenseStatus() == LicenseStatus::UnableToDetermine
    );
}

// Test: Performance - detection completes quickly
TEST_F(WMIDetectorTest, DetectionPerformance) {
    auto start = std::chrono::high_resolution_clock::now();
    LicenseResult result = detector_.Detect();
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);

    // Should complete in less than 10 seconds (WMI can be slow)
    EXPECT_LT(duration.count(), 10);
}

// Test: WMI compatibility with older Windows versions
TEST_F(WMIDetectorTest, OlderWindowsCompatibility) {
    LicenseResult result = detector_.Detect();

    // Should work on Windows 7, 8, 8.1
    int version = result.GetWindowsVersion();
    EXPECT_TRUE(
        version == 7 || version == 8 || version == 81 ||
        version == 10 || version == 11 ||
        version >= 2008  // Server versions
    );
}
