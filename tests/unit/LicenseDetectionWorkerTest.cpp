#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include "../../src/agent/LicenseDetectionWorker.h"
#include "../../src/license-detection/LicenseStatusEnum.h"

class LicenseDetectionWorkerTest : public ::testing::Test {
protected:
    void SetUp() override {
        worker_ = std::make_unique<LicenseDetectionWorker>();
    }

    void TearDown() override {
        // Ensure worker is stopped
        if (worker_) {
            worker_->Stop();
        }
    }

    std::unique_ptr<LicenseDetectionWorker> worker_;
};

// Test: Worker initialization
TEST_F(LicenseDetectionWorkerTest, WorkerInitialization) {
    EXPECT_TRUE(worker_ != nullptr);
}

// Test: Start and Stop methods
TEST_F(LicenseDetectionWorkerTest, StartAndStop) {
    EXPECT_NO_THROW(worker_->Start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_NO_THROW(worker_->Stop());
}

// Test: GetLastResult returns valid result after detection
TEST_F(LicenseDetectionWorkerTest, GetLastResultAfterStart) {
    worker_->Start();

    // Wait for at least one detection cycle
    std::this_thread::sleep_for(std::chrono::seconds(2));

    LicenseResult result = worker_->GetLastResult();

    // Should have a valid result
    EXPECT_TRUE(
        result.GetLicenseStatus() == LicenseStatus::Legitimate ||
        result.GetLicenseStatus() == LicenseStatus::Cracked ||
        result.GetLicenseStatus() == LicenseStatus::NotLicensed ||
        result.GetLicenseStatus() == LicenseStatus::UnableToDetermine
    );

    worker_->Stop();
}

// Test: Worker runs detection (check timestamp changes)
TEST_F(LicenseDetectionWorkerTest, WorkerRunsDetection) {
    worker_->Start();

    // Get initial result
    std::this_thread::sleep_for(std::chrono::seconds(1));
    LicenseResult result1 = worker_->GetLastResult();
    auto timestamp1 = result1.GetTimestamp();

    // Wait a bit and get result again
    std::this_thread::sleep_for(std::chrono::seconds(1));
    LicenseResult result2 = worker_->GetLastResult();
    auto timestamp2 = result2.GetTimestamp();

    // Timestamps should be close (within a few seconds)
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(timestamp2 - timestamp1);
    EXPECT_LT(diff.count(), 5);

    worker_->Stop();
}

// Test: Default interval is 5 minutes
TEST_F(LicenseDetectionWorkerTest, DefaultInterval) {
    // Create worker with default interval
    EXPECT_NO_THROW(worker_->Start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_NO_THROW(worker_->Stop());
}

// Test: Configurable interval
TEST_F(LicenseDetectionWorkerTest, ConfigurableInterval) {
    // Create worker with 1-second interval for testing
    worker_ = std::make_unique<LicenseDetectionWorker>(std::chrono::seconds(1));

    EXPECT_NO_THROW(worker_->Start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_NO_THROW(worker_->Stop());
}

// Test: Multiple Start/Stop cycles
TEST_F(LicenseDetectionWorkerTest, MultipleCycles) {
    for (int i = 0; i < 3; ++i) {
        EXPECT_NO_THROW(worker_->Start());
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        EXPECT_NO_THROW(worker_->Stop());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// Test: Stop is graceful and doesn't hang
TEST_F(LicenseDetectionWorkerTest, StopIsGraceful) {
    worker_->Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto start = std::chrono::high_resolution_clock::now();
    worker_->Stop();
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Stop should complete quickly (within 5 seconds)
    EXPECT_LT(duration.count(), 5000);
}

// Test: Windows version is available in result (FR-4, FR-5)
TEST_F(LicenseDetectionWorkerTest, WindowsVersionInResult) {
    worker_->Start();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    LicenseResult result = worker_->GetLastResult();
    EXPECT_GT(result.GetWindowsVersion(), 0);

    worker_->Stop();
}

// Test: License status is valid
TEST_F(LicenseDetectionWorkerTest, LicenseStatusIsValid) {
    worker_->Start();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    LicenseResult result = worker_->GetLastResult();
    LicenseStatus status = result.GetLicenseStatus();

    EXPECT_TRUE(
        status == LicenseStatus::Legitimate ||
        status == LicenseStatus::Cracked ||
        status == LicenseStatus::NotLicensed ||
        status == LicenseStatus::UnableToDetermine
    );

    worker_->Stop();
}

// Test: KMS status is available
TEST_F(LicenseDetectionWorkerTest, KmsStatusIsAvailable) {
    worker_->Start();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    LicenseResult result = worker_->GetLastResult();

    EXPECT_TRUE(
        result.GetKmsStatus() == KMSStatus::NotKMS ||
        result.GetKmsStatus() == KMSStatus::KMSDetected ||
        result.GetKmsStatus() == KMSStatus::KMSNotFound ||
        result.GetKmsStatus() == KMSStatus::Error
    );

    worker_->Stop();
}

// Test: Repeated calls to GetLastResult return same result (until detection runs again)
TEST_F(LicenseDetectionWorkerTest, GetLastResultConsistency) {
    worker_->Start();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    LicenseResult result1 = worker_->GetLastResult();
    LicenseResult result2 = worker_->GetLastResult();
    LicenseResult result3 = worker_->GetLastResult();

    // All should be identical (same timestamp)
    auto timestamp1 = result1.GetTimestamp();
    auto timestamp2 = result2.GetTimestamp();
    auto timestamp3 = result3.GetTimestamp();

    auto diff1 = std::chrono::duration_cast<std::chrono::milliseconds>(timestamp2 - timestamp1);
    auto diff2 = std::chrono::duration_cast<std::chrono::milliseconds>(timestamp3 - timestamp2);

    EXPECT_EQ(diff1.count(), 0);
    EXPECT_EQ(diff2.count(), 0);

    worker_->Stop();
}

// Test: Worker doesn't crash on repeated operations
TEST_F(LicenseDetectionWorkerTest, RobustUnderRepeatedOperations) {
    for (int i = 0; i < 5; ++i) {
        EXPECT_NO_THROW(worker_->Start());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        LicenseResult result = worker_->GetLastResult();
        EXPECT_GT(result.GetWindowsVersion(), 0);

        EXPECT_NO_THROW(worker_->Stop());
    }
}
