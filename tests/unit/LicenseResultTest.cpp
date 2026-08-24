#include <gtest/gtest.h>
#include <chrono>
#include "../../src/license-detection/LicenseStatusEnum.h"
#include "../../src/license-detection/LicenseResult.h"

class LicenseResultTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Common setup for all tests
    }
};

// Test: Constructor with default values
TEST_F(LicenseResultTest, DefaultConstructor) {
    LicenseResult result;

    // Verify default values
    EXPECT_EQ(result.GetLicenseStatus(), LicenseStatus::UnableToDetermine);
    EXPECT_EQ(result.GetKmsStatus(), KMSStatus::NotKMS);
    EXPECT_EQ(result.GetWindowsVersion(), 0);
    EXPECT_EQ(result.GetWindowsEdition(), "");
    EXPECT_EQ(result.GetWindowsKmsServer(), "");
    EXPECT_EQ(result.GetOfficeKmsServer(), "");
    EXPECT_EQ(result.IsError(), false);
}

// Test: Constructor with parameters
TEST_F(LicenseResultTest, ParameterizedConstructor) {
    auto timestamp = std::chrono::system_clock::now();

    LicenseResult result(
        LicenseStatus::Legitimate,
        timestamp,
        10,  // Windows 10
        "Pro",
        KMSStatus::NotKMS,
        "",  // No KMS server
        "",  // No Office KMS
        false
    );

    EXPECT_EQ(result.GetLicenseStatus(), LicenseStatus::Legitimate);
    EXPECT_EQ(result.GetWindowsVersion(), 10);
    EXPECT_EQ(result.GetWindowsEdition(), "Pro");
    EXPECT_EQ(result.GetKmsStatus(), KMSStatus::NotKMS);
    EXPECT_EQ(result.IsError(), false);
}

// Test: Setters and getters for license status
TEST_F(LicenseResultTest, SetAndGetLicenseStatus) {
    LicenseResult result;

    result.SetLicenseStatus(LicenseStatus::Cracked);
    EXPECT_EQ(result.GetLicenseStatus(), LicenseStatus::Cracked);

    result.SetLicenseStatus(LicenseStatus::NotLicensed);
    EXPECT_EQ(result.GetLicenseStatus(), LicenseStatus::NotLicensed);

    result.SetLicenseStatus(LicenseStatus::Legitimate);
    EXPECT_EQ(result.GetLicenseStatus(), LicenseStatus::Legitimate);
}

// Test: Setters and getters for Windows version and edition
TEST_F(LicenseResultTest, SetAndGetWindowsVersionEdition) {
    LicenseResult result;

    result.SetWindowsVersion(11);
    result.SetWindowsEdition("Enterprise");

    EXPECT_EQ(result.GetWindowsVersion(), 11);
    EXPECT_EQ(result.GetWindowsEdition(), "Enterprise");
}

// Test: Setters and getters for KMS servers
TEST_F(LicenseResultTest, SetAndGetKmsServers) {
    LicenseResult result;

    result.SetKmsStatus(KMSStatus::KMSDetected);
    result.SetWindowsKmsServer("kms.example.com");
    result.SetOfficeKmsServer("office-kms.example.com");

    EXPECT_EQ(result.GetKmsStatus(), KMSStatus::KMSDetected);
    EXPECT_EQ(result.GetWindowsKmsServer(), "kms.example.com");
    EXPECT_EQ(result.GetOfficeKmsServer(), "office-kms.example.com");
}

// Test: Timestamp getter and setter
TEST_F(LicenseResultTest, Timestamp) {
    LicenseResult result;
    auto now = std::chrono::system_clock::now();

    result.SetTimestamp(now);

    // Timestamps should be equal (within microsecond precision)
    auto retrieved = result.GetTimestamp();
    auto diff = std::chrono::duration_cast<std::chrono::microseconds>(
        retrieved - now
    ).count();
    EXPECT_LT(diff, 100);  // Allow small difference due to clock precision
}

// Test: Error flag
TEST_F(LicenseResultTest, ErrorFlag) {
    LicenseResult result;

    EXPECT_EQ(result.IsError(), false);

    result.SetError(true);
    EXPECT_EQ(result.IsError(), true);

    result.SetError(false);
    EXPECT_EQ(result.IsError(), false);
}

// Test: Error message
TEST_F(LicenseResultTest, ErrorMessage) {
    LicenseResult result;

    EXPECT_EQ(result.GetErrorMessage(), "");

    result.SetErrorMessage("API unavailable");
    EXPECT_EQ(result.GetErrorMessage(), "API unavailable");
}

// Test: Copy constructor
TEST_F(LicenseResultTest, CopyConstructor) {
    auto timestamp = std::chrono::system_clock::now();

    LicenseResult original(
        LicenseStatus::Legitimate,
        timestamp,
        10,
        "Pro",
        KMSStatus::NotKMS,
        "",
        "",
        false
    );

    LicenseResult copy(original);

    EXPECT_EQ(copy.GetLicenseStatus(), original.GetLicenseStatus());
    EXPECT_EQ(copy.GetWindowsVersion(), original.GetWindowsVersion());
    EXPECT_EQ(copy.GetWindowsEdition(), original.GetWindowsEdition());
    EXPECT_EQ(copy.IsError(), original.IsError());
}

// Test: Copy assignment
TEST_F(LicenseResultTest, CopyAssignment) {
    auto timestamp = std::chrono::system_clock::now();

    LicenseResult original(
        LicenseStatus::Cracked,
        timestamp,
        11,
        "Enterprise",
        KMSStatus::KMSDetected,
        "kms.example.com",
        "office-kms.example.com",
        false
    );

    LicenseResult assigned;
    assigned = original;

    EXPECT_EQ(assigned.GetLicenseStatus(), LicenseStatus::Cracked);
    EXPECT_EQ(assigned.GetWindowsVersion(), 11);
    EXPECT_EQ(assigned.GetWindowsEdition(), "Enterprise");
    EXPECT_EQ(assigned.GetWindowsKmsServer(), "kms.example.com");
    EXPECT_EQ(assigned.GetOfficeKmsServer(), "office-kms.example.com");
}

// Test: Move constructor
TEST_F(LicenseResultTest, MoveConstructor) {
    auto timestamp = std::chrono::system_clock::now();

    LicenseResult original(
        LicenseStatus::Legitimate,
        timestamp,
        10,
        "Home",
        KMSStatus::NotKMS,
        "",
        "",
        false
    );

    LicenseResult moved(std::move(original));

    EXPECT_EQ(moved.GetLicenseStatus(), LicenseStatus::Legitimate);
    EXPECT_EQ(moved.GetWindowsVersion(), 10);
    EXPECT_EQ(moved.GetWindowsEdition(), "Home");
}

// Test: Move assignment
TEST_F(LicenseResultTest, MoveAssignment) {
    auto timestamp = std::chrono::system_clock::now();

    LicenseResult original(
        LicenseStatus::NotLicensed,
        timestamp,
        7,
        "Professional",
        KMSStatus::Error,
        "",
        "",
        true
    );

    LicenseResult assigned;
    assigned = std::move(original);

    EXPECT_EQ(assigned.GetLicenseStatus(), LicenseStatus::NotLicensed);
    EXPECT_EQ(assigned.GetWindowsVersion(), 7);
    EXPECT_EQ(assigned.IsError(), true);
}

// Test: Multiple fields set and retrieved (FR-13)
TEST_F(LicenseResultTest, CompleteResult) {
    auto timestamp = std::chrono::system_clock::now();

    LicenseResult result;
    result.SetLicenseStatus(LicenseStatus::Legitimate);
    result.SetTimestamp(timestamp);
    result.SetWindowsVersion(10);
    result.SetWindowsEdition("Pro");
    result.SetKmsStatus(KMSStatus::NotKMS);
    result.SetWindowsKmsServer("");
    result.SetOfficeKmsServer("");
    result.SetError(false);
    result.SetErrorMessage("");

    // Verify all fields
    EXPECT_EQ(result.GetLicenseStatus(), LicenseStatus::Legitimate);
    EXPECT_EQ(result.GetWindowsVersion(), 10);
    EXPECT_EQ(result.GetWindowsEdition(), "Pro");
    EXPECT_EQ(result.GetKmsStatus(), KMSStatus::NotKMS);
    EXPECT_EQ(result.GetWindowsKmsServer(), "");
    EXPECT_EQ(result.GetOfficeKmsServer(), "");
    EXPECT_EQ(result.IsError(), false);
    EXPECT_EQ(result.GetErrorMessage(), "");
}
