#include <gtest/gtest.h>
#include "../../src/license-detection/LicenseStatusEnum.h"

// Test: Verify LicenseStatus enums exist
TEST(LicenseStatusEnum, LicenseStatusEnumValues) {
    // These should compile without error if enums are defined
    LicenseStatus legitimate = LicenseStatus::Legitimate;
    LicenseStatus cracked = LicenseStatus::Cracked;
    LicenseStatus notLicensed = LicenseStatus::NotLicensed;
    LicenseStatus unableToDetermine = LicenseStatus::UnableToDetermine;

    // Just verify they compile
    EXPECT_TRUE(true);
}

// Test: Verify LicenseStatusToString conversion works
TEST(LicenseStatusEnum, LicenseStatusToString) {
    EXPECT_EQ(LicenseStatusToString(LicenseStatus::Legitimate), "Legitimate");
    EXPECT_EQ(LicenseStatusToString(LicenseStatus::Cracked), "Cracked");
    EXPECT_EQ(LicenseStatusToString(LicenseStatus::NotLicensed), "NotLicensed");
    EXPECT_EQ(LicenseStatusToString(LicenseStatus::UnableToDetermine), "UnableToDetermine");
}

// Test: Verify KMSStatus enums exist
TEST(LicenseStatusEnum, KMSStatusEnumValues) {
    KMSStatus notKms = KMSStatus::NotKMS;
    KMSStatus kmsDetected = KMSStatus::KMSDetected;
    KMSStatus kmsNotFound = KMSStatus::KMSNotFound;
    KMSStatus error = KMSStatus::Error;

    EXPECT_TRUE(true);
}

// Test: Verify KMSStatusToString conversion works
TEST(LicenseStatusEnum, KMSStatusToString) {
    EXPECT_EQ(KMSStatusToString(KMSStatus::NotKMS), "NotKMS");
    EXPECT_EQ(KMSStatusToString(KMSStatus::KMSDetected), "KMSDetected");
    EXPECT_EQ(KMSStatusToString(KMSStatus::KMSNotFound), "KMSNotFound");
    EXPECT_EQ(KMSStatusToString(KMSStatus::Error), "Error");
}

// Test: Verify Windows version constants exist
TEST(LicenseStatusEnum, WindowsVersionConstants) {
    // These should compile without error if constants are defined
    int version7 = WINDOWS_VERSION_7;
    int version8 = WINDOWS_VERSION_8;
    int version81 = WINDOWS_VERSION_81;
    int version10 = WINDOWS_VERSION_10;
    int version11 = WINDOWS_VERSION_11;

    EXPECT_TRUE(true);
}
