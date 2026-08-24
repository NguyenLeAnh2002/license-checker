#include <gtest/gtest.h>
#include "../../src/agent/NotificationFormatter.h"
#include "../../src/license-detection/LicenseStatusEnum.h"
#include "../../src/license-detection/LicenseResult.h"

class NotificationFormatterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup for notification formatter tests
    }
};

// Test: Formatter creates message for legitimate license
TEST_F(NotificationFormatterTest, LegitimateWindowsLicense) {
    auto timestamp = std::chrono::system_clock::now();
    LicenseResult result(
        LicenseStatus::Legitimate,
        timestamp,
        10,
        "Pro",
        KMSStatus::NotKMS,
        "",
        "",
        false
    );

    std::string message = NotificationFormatter::FormatNotification(result);

    EXPECT_NE(message.find("Legitimate"), std::string::npos);
    EXPECT_NE(message.find("Windows"), std::string::npos);
    EXPECT_TRUE(message.find("Advisory") == std::string::npos);  // No advisory for legitimate
}

// Test: Formatter creates message for cracked license with advisory
TEST_F(NotificationFormatterTest, CrackedWindowsLicenseWithAdvisory) {
    auto timestamp = std::chrono::system_clock::now();
    LicenseResult result(
        LicenseStatus::Cracked,
        timestamp,
        11,
        "Enterprise",
        KMSStatus::NotKMS,
        "",
        "",
        false
    );

    std::string message = NotificationFormatter::FormatNotification(result);

    EXPECT_NE(message.find("Cracked"), std::string::npos);
    EXPECT_NE(message.find("Windows"), std::string::npos);
    // Should include advisory for cracked license
    EXPECT_TRUE(
        message.find("Advisory") != std::string::npos ||
        message.find("contact") != std::string::npos ||
        message.find("IT support") != std::string::npos
    );
}

// Test: Formatter creates message for not licensed
TEST_F(NotificationFormatterTest, NotLicensedWindows) {
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

    std::string message = NotificationFormatter::FormatNotification(result);

    EXPECT_NE(message.find("Not Licensed"), std::string::npos);
    EXPECT_NE(message.find("Windows"), std::string::npos);
    // Should include advisory for unlicensed
    EXPECT_TRUE(
        message.find("Advisory") != std::string::npos ||
        message.find("contact") != std::string::npos ||
        message.find("license") != std::string::npos
    );
}

// Test: Formatter creates message for unable to determine
TEST_F(NotificationFormatterTest, UnableToDetermineStatus) {
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

    std::string message = NotificationFormatter::FormatNotification(result);

    EXPECT_NE(message.find("Unable to determine"), std::string::npos);
    EXPECT_NE(message.find("Windows"), std::string::npos);
}

// Test: Message is non-technical (suitable for office-worker persona)
TEST_F(NotificationFormatterTest, NonTechnicalLanguage) {
    auto timestamp = std::chrono::system_clock::now();
    LicenseResult result(
        LicenseStatus::Cracked,
        timestamp,
        10,
        "Pro",
        KMSStatus::NotKMS,
        "",
        "",
        false
    );

    std::string message = NotificationFormatter::FormatNotification(result);

    // Should NOT contain technical jargon
    EXPECT_TRUE(message.find("registry") == std::string::npos);
    EXPECT_TRUE(message.find("API") == std::string::npos);
    EXPECT_TRUE(message.find("KMS") == std::string::npos);
    EXPECT_TRUE(message.find("SLUI") == std::string::npos);

    // Should be readable and clear
    EXPECT_FALSE(message.empty());
    EXPECT_LT(message.length(), 500);  // Should be concise
}

// Test: Message includes Windows version information
TEST_F(NotificationFormatterTest, IncludesWindowsVersion) {
    auto timestamp = std::chrono::system_clock::now();
    LicenseResult result(
        LicenseStatus::Legitimate,
        timestamp,
        11,
        "Enterprise",
        KMSStatus::NotKMS,
        "",
        "",
        false
    );

    std::string message = NotificationFormatter::FormatNotification(result);

    // Should mention Windows version
    EXPECT_TRUE(
        message.find("Windows") != std::string::npos ||
        message.find("11") != std::string::npos ||
        message.find("Enterprise") != std::string::npos
    );
}

// Test: Message includes Windows edition
TEST_F(NotificationFormatterTest, IncludesWindowsEdition) {
    auto timestamp = std::chrono::system_clock::now();
    LicenseResult result(
        LicenseStatus::Legitimate,
        timestamp,
        10,
        "Pro",
        KMSStatus::NotKMS,
        "",
        "",
        false
    );

    std::string message = NotificationFormatter::FormatNotification(result);

    // Should mention edition
    EXPECT_TRUE(message.find("Pro") != std::string::npos);
}

// Test: All status combinations produce unique messages
TEST_F(NotificationFormatterTest, AllStatusCombinations) {
    auto timestamp = std::chrono::system_clock::now();

    LicenseStatus statuses[] = {
        LicenseStatus::Legitimate,
        LicenseStatus::Cracked,
        LicenseStatus::NotLicensed,
        LicenseStatus::UnableToDetermine
    };

    std::string messages[4];

    for (int i = 0; i < 4; ++i) {
        LicenseResult result(
            statuses[i],
            timestamp,
            10,
            "Pro",
            KMSStatus::NotKMS,
            "",
            "",
            false
        );

        messages[i] = NotificationFormatter::FormatNotification(result);
    }

    // Each message should be different
    for (int i = 0; i < 4; ++i) {
        for (int j = i + 1; j < 4; ++j) {
            EXPECT_NE(messages[i], messages[j]);
        }
    }
}

// Test: Message consistency with JSON output (FR-14)
TEST_F(NotificationFormatterTest, ConsistencyWithJsonOutput) {
    auto timestamp = std::chrono::system_clock::now();
    LicenseResult result(
        LicenseStatus::Legitimate,
        timestamp,
        10,
        "Pro",
        KMSStatus::NotKMS,
        "",
        "",
        false
    );

    std::string notification = NotificationFormatter::FormatNotification(result);
    std::string jsonOutput = result.ToJSON();

    // Both should mention the same license status
    EXPECT_NE(notification.find("Legitimate"), std::string::npos);
    EXPECT_NE(jsonOutput.find("Legitimate"), std::string::npos);

    // Both should mention Windows version
    EXPECT_NE(notification.find("10"), std::string::npos);
    EXPECT_NE(jsonOutput.find("10"), std::string::npos);
}

// Test: KMS information in notification
TEST_F(NotificationFormatterTest, KmsInformationInNotification) {
    auto timestamp = std::chrono::system_clock::now();
    LicenseResult result(
        LicenseStatus::Legitimate,
        timestamp,
        10,
        "Pro",
        KMSStatus::KMSDetected,
        "kms.example.com",
        "",
        false
    );

    std::string message = NotificationFormatter::FormatNotification(result);

    // For non-technical users, KMS details may or may not be shown
    // Just verify the message is well-formed
    EXPECT_FALSE(message.empty());
    EXPECT_NE(message.find("Legitimate"), std::string::npos);
}

// Test: Message length is reasonable (not too long)
TEST_F(NotificationFormatterTest, MessageLengthReasonable) {
    auto timestamp = std::chrono::system_clock::now();
    LicenseResult result(
        LicenseStatus::Cracked,
        timestamp,
        10,
        "Pro",
        KMSStatus::NotKMS,
        "",
        "",
        false
    );

    std::string message = NotificationFormatter::FormatNotification(result);

    // Message should be concise (suitable for popup notification)
    EXPECT_GT(message.length(), 20);   // Should have some content
    EXPECT_LT(message.length(), 1000); // Should be concise
}

// Test: Message is well-formatted and readable
TEST_F(NotificationFormatterTest, FormattingAndReadability) {
    auto timestamp = std::chrono::system_clock::now();
    LicenseResult result(
        LicenseStatus::Legitimate,
        timestamp,
        11,
        "Enterprise",
        KMSStatus::KMSDetected,
        "kms.internal.com",
        "office-kms.internal.com",
        false
    );

    std::string message = NotificationFormatter::FormatNotification(result);

    // Should start with a clear statement
    EXPECT_FALSE(message.empty());
    EXPECT_NE(message.find("Windows"), std::string::npos);

    // Should be human-readable (no weird formatting)
    EXPECT_TRUE(message.find("\\n") == std::string::npos || message.find("\n") != std::string::npos);
}

// Test: Error notification is clear and helpful
TEST_F(NotificationFormatterTest, ErrorNotificationIsClear) {
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
    result.SetErrorMessage("Registry access denied");

    std::string message = NotificationFormatter::FormatNotification(result);

    EXPECT_NE(message.find("Unable to determine"), std::string::npos);
    // Should have helpful guidance (not just the technical error)
    EXPECT_FALSE(message.empty());
}
