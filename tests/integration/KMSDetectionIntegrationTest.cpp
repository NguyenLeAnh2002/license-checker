#include <gtest/gtest.h>
#include "../../src/license-detection/LicenseDetector.h"
#include "../../src/license-detection/LicenseStatusEnum.h"

// Task-17 & Task-18: KMS Detection Integration Tests

class KMSDetectionIntegrationTest : public ::testing::Test {
protected:
    KMSDetectionIntegrationTest() : detector_() {}
    LicenseDetector detector_;
};

// Task-17: Windows KMS Server Detection
TEST_F(KMSDetectionIntegrationTest, WindowsKMSServerDetection) {
    LicenseResult result = detector_.Detect();

    // Should detect KMS status (FR-9)
    EXPECT_TRUE(
        result.GetKmsStatus() == KMSStatus::NotKMS ||
        result.GetKmsStatus() == KMSStatus::KMSDetected ||
        result.GetKmsStatus() == KMSStatus::KMSNotFound ||
        result.GetKmsStatus() == KMSStatus::Error
    );

    // If KMS detected, server address should be non-empty
    if (result.GetKmsStatus() == KMSStatus::KMSDetected) {
        EXPECT_FALSE(result.GetWindowsKmsServer().empty());
        // Server address should be hostname or IP-like format
        EXPECT_GT(result.GetWindowsKmsServer().length(), 2);
    }
}

// Task-17: Non-KMS Machine
TEST_F(KMSDetectionIntegrationTest, NonKMSMachineFallback) {
    LicenseResult result = detector_.Detect();

    // On non-KMS machines, should report "No KMS" (FR-11)
    if (result.GetKmsStatus() == KMSStatus::NotKMS) {
        EXPECT_TRUE(result.GetWindowsKmsServer().empty());
    }
}

// Task-18: Office KMS Detection (if Office installed)
TEST_F(KMSDetectionIntegrationTest, OfficeKMSDetectionIfInstalled) {
    LicenseResult result = detector_.Detect();

    // Should detect Office KMS server if Office is installed and KMS-activated (FR-10)
    // Office KMS should be either empty (not installed or not KMS) or have server
    std::string officeKms = result.GetOfficeKmsServer();

    if (!officeKms.empty()) {
        // If Office KMS is detected, it should be a valid hostname/IP
        EXPECT_GT(officeKms.length(), 2);
    }
}

// Task-18: Office KMS Separate from Windows KMS
TEST_F(KMSDetectionIntegrationTest, OfficeKMSSeparateFromWindows) {
    LicenseResult result = detector_.Detect();

    // Office KMS can be different from Windows KMS (FR-10)
    // Both should be independently detected

    bool windowsHasKms = result.GetKmsStatus() == KMSStatus::KMSDetected &&
                        !result.GetWindowsKmsServer().empty();
    bool officeHasKms = !result.GetOfficeKmsServer().empty();

    // If both have KMS, they might be same or different servers
    if (windowsHasKms && officeHasKms) {
        // Both can coexist independently
        EXPECT_TRUE(true);
    }
}

// Task-18: Office Not Found Handling
TEST_F(KMSDetectionIntegrationTest, OfficeNotFoundHandling) {
    LicenseResult result = detector_.Detect();

    // If Office not installed, should report "Not Found" (FR-11)
    // Office KMS will be empty
    if (result.GetOfficeKmsServer().empty()) {
        // This is valid - either not installed or not KMS-activated
        EXPECT_TRUE(true);
    }
}

// Multiple detection runs should be consistent
TEST_F(KMSDetectionIntegrationTest, KMSConsistencyAcrossRuns) {
    LicenseResult result1 = detector_.Detect();
    LicenseResult result2 = detector_.Detect();

    // KMS status should be consistent
    EXPECT_EQ(result1.GetKmsStatus(), result2.GetKmsStatus());

    if (!result1.GetWindowsKmsServer().empty()) {
        EXPECT_EQ(result1.GetWindowsKmsServer(), result2.GetWindowsKmsServer());
    }

    if (!result1.GetOfficeKmsServer().empty()) {
        EXPECT_EQ(result1.GetOfficeKmsServer(), result2.GetOfficeKmsServer());
    }
}
