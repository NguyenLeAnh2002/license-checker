#pragma once

#include "../license-detection/LicenseDetector.h"
#include "../license-detection/LicenseResult.h"
#include "../license-detection/DetectionLogger.h"
#include "../common/NamedPipeServer.h"
#include "ServerReporter.h"
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <memory>

// Background worker thread that runs license detection on a configurable interval (FR-16)
// Implements task serialization: if detection is still running, queues next request instead of concurrent runs
class LicenseDetectionWorker {
public:
    // Constructor with default 5-minute interval
    LicenseDetectionWorker();

    // Constructor with custom interval
    explicit LicenseDetectionWorker(std::chrono::seconds interval);

    // Destructor
    ~LicenseDetectionWorker();

    // Delete copy semantics
    LicenseDetectionWorker(const LicenseDetectionWorker&) = delete;
    LicenseDetectionWorker& operator=(const LicenseDetectionWorker&) = delete;

    // Allow move semantics
    LicenseDetectionWorker(LicenseDetectionWorker&&) noexcept;
    LicenseDetectionWorker& operator=(LicenseDetectionWorker&&) noexcept;

    // Start the background worker thread
    void Start();

    // Stop the background worker thread gracefully
    void Stop();

    // Get the last detection result (thread-safe)
    LicenseResult GetLastResult() const;

private:
    // Worker thread function
    void WorkerThreadLoop();

    // Perform a single detection cycle with logging
    void PerformDetectionCycle();

    // Members
    std::unique_ptr<LicenseDetector> detector_;
    std::unique_ptr<DetectionLogger> logger_;
    std::unique_ptr<ServerReporter> reporter_;  // must be declared after logger_ (depends on it at construction)
    std::unique_ptr<NamedPipeServer> pipeServer_;
    std::thread workerThread_;
    mutable std::mutex resultMutex_;
    LicenseResult lastResult_;
    std::atomic<bool> isRunning_;
    std::atomic<bool> stopRequested_;
    std::chrono::seconds detectionInterval_;
    std::atomic<bool> detectionInProgress_;
    std::atomic<bool> detectionQueued_;
};
