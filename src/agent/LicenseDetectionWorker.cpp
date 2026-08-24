#include "LicenseDetectionWorker.h"
#include "../common/SharedLicenseDataWriter.h"
#include <iostream>
#include <winsock2.h>

// Default 5-minute interval
LicenseDetectionWorker::LicenseDetectionWorker()
    : LicenseDetectionWorker(std::chrono::seconds(300)) {
}

// Constructor with custom interval
LicenseDetectionWorker::LicenseDetectionWorker(std::chrono::seconds interval)
    : detector_(std::make_unique<LicenseDetector>()),
      logger_(std::make_unique<DetectionLogger>("license-detection.log")),
      reporter_(std::make_unique<ServerReporter>(*logger_)),
      pipeServer_(std::make_unique<NamedPipeServer>("LicenseChecker")),
      isRunning_(false),
      stopRequested_(false),
      detectionInterval_(interval),
      detectionInProgress_(false),
      detectionQueued_(false) {
    // Set up request handler for pipe server
    pipeServer_->SetRequestHandler([this](const std::string& command, const std::string& payload) {
        if (command == "GetLicenseData") {
            std::lock_guard<std::mutex> lock(resultMutex_);
            return NamedPipeServer::BuildLicenseDataResponse(lastResult_);
        } else if (command == "RequestImmediateCheck") {
            // Queue immediate detection
            detectionQueued_ = true;
            return NamedPipeServer::BuildSuccessResponse("Immediate check queued");
        } else {
            return NamedPipeServer::BuildErrorResponse("Unknown command");
        }
    });
}

// Destructor
LicenseDetectionWorker::~LicenseDetectionWorker() {
    Stop();
}

// Move constructor
LicenseDetectionWorker::LicenseDetectionWorker(LicenseDetectionWorker&& other) noexcept
    : detector_(std::move(other.detector_)),
      logger_(std::move(other.logger_)),
      reporter_(std::move(other.reporter_)),
      lastResult_(other.lastResult_),
      isRunning_(other.isRunning_.load()),
      stopRequested_(other.stopRequested_.load()),
      detectionInterval_(other.detectionInterval_),
      detectionInProgress_(other.detectionInProgress_.load()),
      detectionQueued_(other.detectionQueued_.load()) {
    // Note: thread cannot be moved, so we don't move workerThread_
}

// Move assignment
LicenseDetectionWorker& LicenseDetectionWorker::operator=(LicenseDetectionWorker&& other) noexcept {
    if (this != &other) {
        Stop();
        detector_ = std::move(other.detector_);
        logger_ = std::move(other.logger_);
        reporter_ = std::move(other.reporter_);
        lastResult_ = other.lastResult_;
        isRunning_ = other.isRunning_.load();
        stopRequested_ = other.stopRequested_.load();
        detectionInterval_ = other.detectionInterval_;
        detectionInProgress_ = other.detectionInProgress_.load();
        detectionQueued_ = other.detectionQueued_.load();
    }
    return *this;
}

// Start the background worker thread
void LicenseDetectionWorker::Start() {
    if (isRunning_) {
        return;  // Already running
    }

    stopRequested_ = false;
    isRunning_ = true;
    detectionInProgress_ = false;
    detectionQueued_ = false;

    // Start named pipe server for UI communication
    if (pipeServer_) {
        pipeServer_->Start();
    }

    workerThread_ = std::thread(&LicenseDetectionWorker::WorkerThreadLoop, this);
}

// Stop the background worker thread gracefully
void LicenseDetectionWorker::Stop() {
    if (!isRunning_) {
        return;  // Not running
    }

    stopRequested_ = true;

    // Stop pipe server
    if (pipeServer_) {
        pipeServer_->Stop();
    }

    // Wait for thread to finish
    if (workerThread_.joinable()) {
        workerThread_.join();
    }

    isRunning_ = false;
}

// Get the last detection result (thread-safe)
LicenseResult LicenseDetectionWorker::GetLastResult() const {
    std::lock_guard<std::mutex> lock(resultMutex_);
    return lastResult_;
}

// Worker thread loop
void LicenseDetectionWorker::WorkerThreadLoop() {
    // Perform initial detection immediately
    PerformDetectionCycle();

    // Then loop on interval
    while (!stopRequested_) {
        // Sleep for the detection interval
        // Use small sleep increments to allow quick shutdown
        for (int i = 0; i < detectionInterval_.count() * 10 && !stopRequested_; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Perform detection if not stopped
        if (!stopRequested_) {
            PerformDetectionCycle();

            // Handle queued detection if previous cycle was still running
            if (detectionQueued_ && !detectionInProgress_) {
                detectionQueued_ = false;
                PerformDetectionCycle();
            }
        }
    }
}

// Perform a single detection cycle with logging
void LicenseDetectionWorker::PerformDetectionCycle() {
    // Task serialization: if detection is already in progress, queue it instead
    if (detectionInProgress_) {
        detectionQueued_ = true;
        return;
    }

    detectionInProgress_ = true;

    try {
        // Perform detection
        LicenseResult result = detector_->Detect();

        // Store result (thread-safe)
        {
            std::lock_guard<std::mutex> lock(resultMutex_);
            lastResult_ = result;
        }

        // Log result
        if (result.IsError()) {
            logger_->LogError(result.GetErrorMessage());
        } else {
            logger_->LogSuccess(result);
        }

        // Report to remote server, if configured (no-op otherwise; never throws)
        if (reporter_) {
            reporter_->SendReport(result);
        }
    }
    catch (const std::exception& ex) {
        logger_->LogError(std::string("Detection cycle failed: ") + ex.what());
    }
    catch (...) {
        logger_->LogError("Detection cycle failed: Unknown exception");
    }

    detectionInProgress_ = false;
}
