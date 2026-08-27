#pragma once

#include "../license-detection/LicenseDetector.h"
#include "../license-detection/LicenseResult.h"
#include "../license-detection/DetectionLogger.h"
#include "ServerReporter.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>

// Runs license detection in-process on a background thread, on a fixed
// interval, and pushes each result to the owner via callback (invoked on
// the worker thread - the callback must marshal back to the UI thread
// itself, e.g. via PostMessage). Replaces the old cross-process Agent +
// Named Pipe design now that detection lives in the same process as the UI.
class DetectionWorker {
public:
    using ResultCallback = std::function<void(const LicenseResult&)>;

    DetectionWorker(std::chrono::seconds interval, ResultCallback onResult);
    ~DetectionWorker();

    DetectionWorker(const DetectionWorker&) = delete;
    DetectionWorker& operator=(const DetectionWorker&) = delete;

    void Start();
    void Stop();

    // Wakes the worker immediately instead of waiting out the rest of the
    // current interval. Safe to call from any thread.
    void RequestImmediateCheck();

private:
    void ThreadLoop();

    std::unique_ptr<LicenseDetector> detector_;
    std::unique_ptr<DetectionLogger> logger_;
    std::unique_ptr<ServerReporter> reporter_;  // depends on logger_, must be declared after it
    ResultCallback onResult_;
    std::chrono::seconds interval_;

    std::thread thread_;
    std::mutex cvMutex_;
    std::condition_variable cv_;
    std::atomic<bool> stopRequested_;
    std::atomic<bool> checkRequested_;
};
