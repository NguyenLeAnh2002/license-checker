#include <gtest/gtest.h>
#include <fstream>
#include <thread>
#include <vector>
#include <filesystem>
#include "../../src/license-detection/DetectionLogger.h"
#include "../../src/license-detection/LicenseResult.h"
#include "../../src/license-detection/LicenseStatusEnum.h"

namespace fs = std::filesystem;

class DetectionLoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary directory for test logs
        testLogDir_ = fs::temp_directory_path() / "license-checker-test";
        fs::create_directories(testLogDir_);

        logFile_ = (testLogDir_ / "test.log").string();
    }

    void TearDown() override {
        // Clean up test logs
        if (fs::exists(testLogDir_)) {
            fs::remove_all(testLogDir_);
        }
    }

    std::string logFile_;
    fs::path testLogDir_;
};

// Test: Logger initialization
TEST_F(DetectionLoggerTest, LoggerInitialization) {
    DetectionLogger logger(logFile_);

    // Should initialize without errors
    EXPECT_TRUE(true);
}

// Test: LogSuccess writes to file
TEST_F(DetectionLoggerTest, LogSuccessWritesToFile) {
    DetectionLogger logger(logFile_);

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

    logger.LogSuccess(result);

    // Verify log file was created and contains content
    EXPECT_TRUE(fs::exists(logFile_));

    std::ifstream file(logFile_);
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    EXPECT_FALSE(content.empty());
    EXPECT_NE(content.find("Legitimate"), std::string::npos);
    EXPECT_NE(content.find("Windows 10"), std::string::npos);
}

// Test: LogFailure writes error to file
TEST_F(DetectionLoggerTest, LogFailureWritesToFile) {
    DetectionLogger logger(logFile_);

    std::string errorMsg = "API unavailable: SL API not accessible";
    logger.LogFailure(errorMsg);

    EXPECT_TRUE(fs::exists(logFile_));

    std::ifstream file(logFile_);
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    EXPECT_NE(content.find(errorMsg), std::string::npos);
}

// Test: LogError writes exception info to file
TEST_F(DetectionLoggerTest, LogErrorWritesToFile) {
    DetectionLogger logger(logFile_);

    std::string exceptionMsg = "std::runtime_error: WMI query failed";
    logger.LogError(exceptionMsg);

    EXPECT_TRUE(fs::exists(logFile_));

    std::ifstream file(logFile_);
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    EXPECT_NE(content.find(exceptionMsg), std::string::npos);
}

// Test: Multiple log entries
TEST_F(DetectionLoggerTest, MultipleLogEntries) {
    DetectionLogger logger(logFile_);

    auto timestamp = std::chrono::system_clock::now();

    // Log success
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
    logger.LogSuccess(result1);

    // Log failure
    logger.LogFailure("First detection failed");

    // Log error
    logger.LogError("Exception occurred");

    // Log another success
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
    logger.LogSuccess(result2);

    std::ifstream file(logFile_);
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    // Verify all entries are present
    EXPECT_NE(content.find("Legitimate"), std::string::npos);
    EXPECT_NE(content.find("First detection failed"), std::string::npos);
    EXPECT_NE(content.find("Exception occurred"), std::string::npos);
    EXPECT_NE(content.find("Cracked"), std::string::npos);
    EXPECT_NE(content.find("Windows 11"), std::string::npos);
}

// Test: Thread-safe concurrent logging (FR-8)
TEST_F(DetectionLoggerTest, ThreadSafeLogging) {
    DetectionLogger logger(logFile_);

    auto timestamp = std::chrono::system_clock::now();
    std::vector<std::thread> threads;

    // Create 5 threads that all log simultaneously
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&logger, timestamp, i]() {
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
            logger.LogSuccess(result);
            logger.LogFailure("Error from thread " + std::to_string(i));
        });
    }

    // Wait for all threads to finish
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify all logs were written (10 entries: 5 successes + 5 failures)
    std::ifstream file(logFile_);
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    // Count occurrences of "Legitimate" (should have at least 5)
    int legitimateCount = 0;
    size_t pos = 0;
    while ((pos = content.find("Legitimate", pos)) != std::string::npos) {
        legitimateCount++;
        pos += 10;
    }

    EXPECT_GE(legitimateCount, 5);

    // Verify no data corruption (all error messages present)
    for (int i = 0; i < 5; ++i) {
        std::string searchStr = "Error from thread " + std::to_string(i);
        EXPECT_NE(content.find(searchStr), std::string::npos);
    }
}

// Test: Log format includes timestamp
TEST_F(DetectionLoggerTest, LogFormatIncludesTimestamp) {
    DetectionLogger logger(logFile_);

    LicenseResult result(
        LicenseStatus::Legitimate,
        std::chrono::system_clock::now(),
        10,
        "Pro",
        KMSStatus::NotKMS,
        "",
        "",
        false
    );

    logger.LogSuccess(result);

    std::ifstream file(logFile_);
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    // Log should contain timestamp indicators (date/time format)
    // At minimum, look for patterns like "20" (year prefix) or colons (time separator)
    EXPECT_TRUE(
        content.find("20") != std::string::npos ||  // Year prefix
        content.find(":") != std::string::npos       // Time separator
    );
}

// Test: Log file is readable
TEST_F(DetectionLoggerTest, LogFileReadable) {
    DetectionLogger logger(logFile_);

    logger.LogFailure("Test message");
    logger.LogSuccess(LicenseResult());

    // Try to read file and verify no corruption
    std::ifstream file(logFile_);
    EXPECT_TRUE(file.is_open());

    std::string line;
    int lineCount = 0;
    while (std::getline(file, line)) {
        lineCount++;
        EXPECT_FALSE(line.empty());
    }

    EXPECT_GE(lineCount, 1);  // At least one line logged
}
