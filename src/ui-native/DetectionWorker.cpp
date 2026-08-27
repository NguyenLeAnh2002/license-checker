#include "DetectionWorker.h"
#include <windows.h>

namespace {

// std::ofstream("license-detection.log") resolves relative to the process's
// current working directory, which isn't always the executable's own folder
// (e.g. launched via a shortcut with a different "Start in", or a scheduled
// task) - some machines in the field were ending up with the log somewhere
// other than next to the exe, making it useless for diagnosing why they
// weren't reporting. Anchor it to the exe's actual directory instead.
std::string GetLogFilePath() {
    char pathBuf[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, pathBuf, MAX_PATH);
    if (len == 0 || len == MAX_PATH) {
        return "license-detection.log";  // Fall back to the old CWD-relative behavior.
    }
    std::string path(pathBuf, len);
    size_t pos = path.find_last_of("\\/");
    if (pos == std::string::npos) {
        return "license-detection.log";
    }
    return path.substr(0, pos) + "\\license-detection.log";
}

} // namespace

DetectionWorker::DetectionWorker(std::chrono::seconds interval, ResultCallback onResult)
    : detector_(std::make_unique<LicenseDetector>()),
      logger_(std::make_unique<DetectionLogger>(GetLogFilePath())),
      reporter_(std::make_unique<ServerReporter>(*logger_)),
      onResult_(std::move(onResult)),
      interval_(interval),
      stopRequested_(false),
      checkRequested_(false) {
}

DetectionWorker::~DetectionWorker() {
    Stop();
}

void DetectionWorker::Start() {
    if (thread_.joinable()) {
        return;  // Already running.
    }
    stopRequested_ = false;
    thread_ = std::thread(&DetectionWorker::ThreadLoop, this);
}

void DetectionWorker::Stop() {
    stopRequested_ = true;
    cv_.notify_one();
    if (thread_.joinable()) {
        thread_.join();
    }
}

void DetectionWorker::RequestImmediateCheck() {
    {
        std::lock_guard<std::mutex> lock(cvMutex_);
        checkRequested_ = true;
    }
    cv_.notify_one();
}

void DetectionWorker::ThreadLoop() {
    while (!stopRequested_) {
        try {
            LicenseResult result = detector_->Detect();

            if (result.IsError()) {
                logger_->LogError(result.GetErrorMessage());
            } else {
                logger_->LogSuccess(result);
            }

            if (reporter_) {
                reporter_->SendReport(result);
            }

            if (onResult_) {
                onResult_(result);
            }
        } catch (const std::exception& ex) {
            logger_->LogError(std::string("Detection cycle failed: ") + ex.what());
        } catch (...) {
            logger_->LogError("Detection cycle failed: Unknown exception");
        }

        std::unique_lock<std::mutex> lock(cvMutex_);
        cv_.wait_for(lock, interval_, [this] {
            return stopRequested_.load() || checkRequested_.load();
        });
        checkRequested_ = false;
    }
}
