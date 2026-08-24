#pragma once

#include <string>
#include <mutex>
#include <fstream>
#include "LicenseResult.h"

// Thread-safe logger for license detection attempts and results (FR-8)
class DetectionLogger {
public:
    // Constructor: specify log file path
    explicit DetectionLogger(const std::string& logFilePath);

    // Destructor
    ~DetectionLogger();

    // Delete copy semantics (logger holds file handle)
    DetectionLogger(const DetectionLogger&) = delete;
    DetectionLogger& operator=(const DetectionLogger&) = delete;

    // Allow move semantics
    DetectionLogger(DetectionLogger&& other) noexcept;
    DetectionLogger& operator=(DetectionLogger&& other) noexcept;

    // Logging methods
    // Log successful detection with result details
    void LogSuccess(const LicenseResult& result);

    // Log a detection failure with error message
    void LogFailure(const std::string& errorMessage);

    // Log an exception with exception details
    void LogError(const std::string& exceptionMessage);

    // Log an informational message (e.g. server report status)
    void LogInfo(const std::string& message);

private:
    std::string logFilePath_;
    std::ofstream logFile_;
    mutable std::mutex logMutex_;  // For thread-safe file access

    // Helper: Write a formatted log entry
    void WriteLogEntry(const std::string& level, const std::string& message);

    // Helper: Get current timestamp as string
    static std::string GetCurrentTimestamp();
};
