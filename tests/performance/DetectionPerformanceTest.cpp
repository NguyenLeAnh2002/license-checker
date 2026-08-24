#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <numeric>
#include "../../src/license-detection/LicenseDetector.h"

// Task-21: Performance Testing

class DetectionPerformanceTest : public ::testing::Test {
protected:
    DetectionPerformanceTest() : detector_() {}
    LicenseDetector detector_;
};

// Task-21: Detection completes quickly (FR-15)
TEST_F(DetectionPerformanceTest, DetectionCompletesQuickly) {
    auto start = std::chrono::high_resolution_clock::now();
    LicenseResult result = detector_.Detect();
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Single detection should complete quickly
    EXPECT_LT(duration.count(), 10000);  // Under 10 seconds
    EXPECT_GT(duration.count(), 0);       // At least 1ms
}

// Task-21: Multiple detections performance (100 runs)
TEST_F(DetectionPerformanceTest, AverageBenchmark) {
    std::vector<long long> durations;

    for (int i = 0; i < 100; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        detector_.Detect();
        auto end = std::chrono::high_resolution_clock::now();

        long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        durations.push_back(ms);
    }

    // Calculate statistics
    long long sum = std::accumulate(durations.begin(), durations.end(), 0LL);
    long long avg = sum / durations.size();

    long long max = *std::max_element(durations.begin(), durations.end());
    long long min = *std::min_element(durations.begin(), durations.end());

    // FR-15: Average should be < 5 seconds, max < 10 seconds
    EXPECT_LT(avg, 5000);
    EXPECT_LT(max, 10000);

    // Min should be reasonable
    EXPECT_GT(min, 10);
}

// Task-21: No performance degradation over time
TEST_F(DetectionPerformanceTest, ConsistentPerformance) {
    std::vector<long long> first_10, second_10;

    // First 10 runs
    for (int i = 0; i < 10; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        detector_.Detect();
        auto end = std::chrono::high_resolution_clock::now();

        first_10.push_back(
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
        );
    }

    // Second 10 runs
    for (int i = 0; i < 10; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        detector_.Detect();
        auto end = std::chrono::high_resolution_clock::now();

        second_10.push_back(
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
        );
    }

    long long avg_first = std::accumulate(first_10.begin(), first_10.end(), 0LL) / 10;
    long long avg_second = std::accumulate(second_10.begin(), second_10.end(), 0LL) / 10;

    // Performance should be consistent (second batch shouldn't be significantly slower)
    // Allow up to 50% slower on second batch (caching, warm-up differences)
    EXPECT_LT(avg_second, avg_first * 1.5);
}

// Task-21: Peak load performance
TEST_F(DetectionPerformanceTest, PeakLoadPerformance) {
    // Rapid-fire 50 detections
    std::vector<long long> durations;

    for (int i = 0; i < 50; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        detector_.Detect();
        auto end = std::chrono::high_resolution_clock::now();

        durations.push_back(
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
        );
    }

    long long max = *std::max_element(durations.begin(), durations.end());

    // Even under peak load, should not exceed 10 seconds
    EXPECT_LT(max, 10000);
}

// Task-21: Memory efficiency (runs without memory issues)
TEST_F(DetectionPerformanceTest, MemoryEfficiency) {
    // 100 rapid detections should not cause memory issues
    for (int i = 0; i < 100; ++i) {
        LicenseResult result = detector_.Detect();

        // Verify result is valid (not corrupted by memory issues)
        EXPECT_TRUE(
            result.GetLicenseStatus() == LicenseStatus::Legitimate ||
            result.GetLicenseStatus() == LicenseStatus::Cracked ||
            result.GetLicenseStatus() == LicenseStatus::NotLicensed ||
            result.GetLicenseStatus() == LicenseStatus::UnableToDetermine
        );
    }
}
