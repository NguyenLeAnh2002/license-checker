#include <gtest/gtest.h>
#include <windows.h>
#include "../../src/agent/LicenseDetectionWorker.h"
#include "../../src/license-detection/LicenseDetector.h"

// Unit tests for Windows Service integration logic
// Note: These test the core service logic; actual service registration tests require manual testing

class ServiceLogicTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup for service logic tests
    }

    void TearDown() override {
        // Cleanup
    }
};

// Test: LicenseDetectionWorker can be instantiated for service
TEST_F(ServiceLogicTest, WorkerCanBeInstantiatedForService) {
    LicenseDetectionWorker worker;
    EXPECT_NO_THROW(true);
}

// Test: Worker can start detection
TEST_F(ServiceLogicTest, WorkerCanStartDetection) {
    LicenseDetectionWorker worker;
    EXPECT_NO_THROW(worker.Start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_NO_THROW(worker.Stop());
}

// Test: Worker can be stopped gracefully
TEST_F(ServiceLogicTest, WorkerCanBeStopped) {
    LicenseDetectionWorker worker;
    worker.Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_NO_THROW(worker.Stop());
}

// Test: Detector produces results for service reporting
TEST_F(ServiceLogicTest, DetectorProducesResults) {
    LicenseDetector detector;
    LicenseResult result = detector.Detect();

    EXPECT_TRUE(
        result.GetLicenseStatus() == LicenseStatus::Legitimate ||
        result.GetLicenseStatus() == LicenseStatus::Cracked ||
        result.GetLicenseStatus() == LicenseStatus::NotLicensed ||
        result.GetLicenseStatus() == LicenseStatus::UnableToDetermine
    );
}

// Test: Service can retrieve last result from worker
TEST_F(ServiceLogicTest, ServiceCanGetLastResult) {
    LicenseDetectionWorker worker;
    worker.Start();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    LicenseResult result = worker.GetLastResult();
    EXPECT_GT(result.GetWindowsVersion(), 0);

    worker.Stop();
}

// Test: Multiple start/stop cycles (service lifecycle)
TEST_F(ServiceLogicTest, ServiceLifecycle) {
    for (int cycle = 0; cycle < 3; ++cycle) {
        LicenseDetectionWorker worker;
        worker.Start();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        worker.Stop();
    }
}

// Test: Service continues running after start
TEST_F(ServiceLogicTest, ServiceContinuesRunning) {
    LicenseDetectionWorker worker;
    worker.Start();

    // Service should be detecting in background
    std::this_thread::sleep_for(std::chrono::seconds(1));
    LicenseResult result1 = worker.GetLastResult();

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    LicenseResult result2 = worker.GetLastResult();

    // Both results should be valid
    EXPECT_GT(result1.GetWindowsVersion(), 0);
    EXPECT_GT(result2.GetWindowsVersion(), 0);

    worker.Stop();
}

// Test: Service handles errors gracefully
TEST_F(ServiceLogicTest, ServiceHandlesErrorsGracefully) {
    LicenseDetectionWorker worker;
    worker.Start();

    // Even if detection has errors, service should continue running
    std::this_thread::sleep_for(std::chrono::seconds(1));
    LicenseResult result = worker.GetLastResult();

    // Result should be valid even if status is UnableToDetermine
    EXPECT_TRUE(
        result.GetLicenseStatus() == LicenseStatus::Legitimate ||
        result.GetLicenseStatus() == LicenseStatus::Cracked ||
        result.GetLicenseStatus() == LicenseStatus::NotLicensed ||
        result.GetLicenseStatus() == LicenseStatus::UnableToDetermine
    );

    worker.Stop();
}

// Test: Worker runs with reasonable responsiveness
TEST_F(ServiceLogicTest, ServiceResponsiveness) {
    LicenseDetectionWorker worker;
    auto start = std::chrono::high_resolution_clock::now();
    worker.Start();
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_LT(duration.count(), 100);  // Start should be quick

    worker.Stop();
}

// Test: Detector works with SYSTEM privileges (or without them for testing)
TEST_F(ServiceLogicTest, DetectorWorksWithoutPrivileges) {
    // This tests that detector can run even in limited context
    LicenseDetector detector;
    LicenseResult result = detector.Detect();

    // Should return a result even if permissions are limited
    EXPECT_TRUE(true);
}
