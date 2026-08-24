#include <gtest/gtest.h>
#include <string>
#include <sstream>
#include "../../src/license-detection/LicenseResult.h"
#include "../../src/license-detection/LicenseStatusEnum.h"

class LicenseResultJsonTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a sample result for testing
        auto timestamp = std::chrono::system_clock::now();
        sampleResult_ = LicenseResult(
            LicenseStatus::Legitimate,
            timestamp,
            10,
            "Pro",
            KMSStatus::NotKMS,
            "",
            "",
            false
        );
    }

    LicenseResult sampleResult_;
};

// Test: ToJSON method exists and returns non-empty string
TEST_F(LicenseResultJsonTest, ToJsonReturnsString) {
    std::string json = sampleResult_.ToJSON();
    EXPECT_FALSE(json.empty());
}

// Test: JSON contains license status field
TEST_F(LicenseResultJsonTest, JsonContainsLicenseStatus) {
    std::string json = sampleResult_.ToJSON();
    EXPECT_NE(json.find("\"licenseStatus\""), std::string::npos);
    EXPECT_NE(json.find("Legitimate"), std::string::npos);
}

// Test: JSON contains Windows version field
TEST_F(LicenseResultJsonTest, JsonContainsWindowsVersion) {
    std::string json = sampleResult_.ToJSON();
    EXPECT_NE(json.find("\"windowsVersion\""), std::string::npos);
    EXPECT_NE(json.find("10"), std::string::npos);
}

// Test: JSON contains Windows edition field
TEST_F(LicenseResultJsonTest, JsonContainsWindowsEdition) {
    std::string json = sampleResult_.ToJSON();
    EXPECT_NE(json.find("\"windowsEdition\""), std::string::npos);
    EXPECT_NE(json.find("Pro"), std::string::npos);
}

// Test: JSON contains KMS status field
TEST_F(LicenseResultJsonTest, JsonContainsKmsStatus) {
    std::string json = sampleResult_.ToJSON();
    EXPECT_NE(json.find("\"kmsStatus\""), std::string::npos);
    EXPECT_NE(json.find("NotKMS"), std::string::npos);
}

// Test: JSON contains timestamp field (ISO 8601 format)
TEST_F(LicenseResultJsonTest, JsonContainsTimestamp) {
    std::string json = sampleResult_.ToJSON();
    EXPECT_NE(json.find("\"timestamp\""), std::string::npos);
}

// Test: JSON contains Windows KMS server field
TEST_F(LicenseResultJsonTest, JsonContainsWindowsKmsServer) {
    std::string json = sampleResult_.ToJSON();
    EXPECT_NE(json.find("\"windowsKmsServer\""), std::string::npos);
}

// Test: JSON contains Office KMS server field
TEST_F(LicenseResultJsonTest, JsonContainsOfficeKmsServer) {
    std::string json = sampleResult_.ToJSON();
    EXPECT_NE(json.find("\"officeKmsServer\""), std::string::npos);
}

// Test: JSON is valid (starts with { and ends with })
TEST_F(LicenseResultJsonTest, JsonIsValidObject) {
    std::string json = sampleResult_.ToJSON();
    EXPECT_EQ(json.front(), '{');
    EXPECT_EQ(json.back(), '}');
}

// Test: JSON with Cracked status
TEST_F(LicenseResultJsonTest, JsonWithCrackedStatus) {
    auto timestamp = std::chrono::system_clock::now();
    LicenseResult result(
        LicenseStatus::Cracked,
        timestamp,
        11,
        "Enterprise",
        KMSStatus::KMSDetected,
        "kms.example.com",
        "office-kms.example.com",
        false
    );

    std::string json = result.ToJSON();
    EXPECT_NE(json.find("Cracked"), std::string::npos);
    EXPECT_NE(json.find("Enterprise"), std::string::npos);
    EXPECT_NE(json.find("kms.example.com"), std::string::npos);
}

// Test: JSON with NotLicensed status
TEST_F(LicenseResultJsonTest, JsonWithNotLicensedStatus) {
    auto timestamp = std::chrono::system_clock::now();
    LicenseResult result(
        LicenseStatus::NotLicensed,
        timestamp,
        7,
        "Professional",
        KMSStatus::NotKMS,
        "",
        "",
        false
    );

    std::string json = result.ToJSON();
    EXPECT_NE(json.find("NotLicensed"), std::string::npos);
    EXPECT_NE(json.find("Professional"), std::string::npos);
}

// Test: JSON with KMS detected
TEST_F(LicenseResultJsonTest, JsonWithKmsDetected) {
    auto timestamp = std::chrono::system_clock::now();
    LicenseResult result(
        LicenseStatus::Legitimate,
        timestamp,
        10,
        "Pro",
        KMSStatus::KMSDetected,
        "kms.internal.company.com",
        "",
        false
    );

    std::string json = result.ToJSON();
    EXPECT_NE(json.find("KMSDetected"), std::string::npos);
    EXPECT_NE(json.find("kms.internal.company.com"), std::string::npos);
}

// Test: JSON with error flag
TEST_F(LicenseResultJsonTest, JsonWithErrorFlag) {
    auto timestamp = std::chrono::system_clock::now();
    LicenseResult result(
        LicenseStatus::UnableToDetermine,
        timestamp,
        0,
        "Unknown",
        KMSStatus::Error,
        "",
        "",
        true
    );
    result.SetErrorMessage("Detection failed");

    std::string json = result.ToJSON();
    EXPECT_NE(json.find("\"isError\""), std::string::npos);
    EXPECT_NE(json.find("true"), std::string::npos);
    EXPECT_NE(json.find("errorMessage"), std::string::npos);
    EXPECT_NE(json.find("Detection failed"), std::string::npos);
}

// Test: JSON can be parsed back (basic deserialization test)
TEST_F(LicenseResultJsonTest, JsonCanBeParsedBack) {
    std::string json = sampleResult_.ToJSON();

    // Verify it contains enough information to reconstruct
    EXPECT_NE(json.find("\"licenseStatus\""), std::string::npos);
    EXPECT_NE(json.find("\"windowsVersion\""), std::string::npos);
    EXPECT_NE(json.find("\"timestamp\""), std::string::npos);

    // The JSON should be parseable by any JSON parser
    // (This test just verifies the structure is sound)
    EXPECT_FALSE(json.empty());
}

// Test: Multiple results produce different JSON
TEST_F(LicenseResultJsonTest, DifferentResultsProduceDifferentJson) {
    auto timestamp = std::chrono::system_clock::now();

    LicenseResult result1(
        LicenseStatus::Legitimate,
        timestamp,
        10,
        "Pro",
        KMSStatus::NotKMS,
        "",
        "",
        false
    );

    LicenseResult result2(
        LicenseStatus::Cracked,
        timestamp,
        11,
        "Enterprise",
        KMSStatus::KMSDetected,
        "kms.example.com",
        "",
        false
    );

    std::string json1 = result1.ToJSON();
    std::string json2 = result2.ToJSON();

    EXPECT_NE(json1, json2);
}

// Test: JSON contains version field for schema compatibility
TEST_F(LicenseResultJsonTest, JsonContainsSchemaVersion) {
    std::string json = sampleResult_.ToJSON();
    EXPECT_NE(json.find("\"version\""), std::string::npos);
}
