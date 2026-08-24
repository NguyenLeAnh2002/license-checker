#include "DetectionLogger.h"
#include <sstream>
#include <iomanip>
#include <ctime>

// Constructor
DetectionLogger::DetectionLogger(const std::string& logFilePath)
    : logFilePath_(logFilePath) {
    // Open log file in append mode
    logFile_.open(logFilePath_, std::ios::app);
}

// Destructor
DetectionLogger::~DetectionLogger() {
    if (logFile_.is_open()) {
        logFile_.close();
    }
}

// Move constructor
DetectionLogger::DetectionLogger(DetectionLogger&& other) noexcept
    : logFilePath_(std::move(other.logFilePath_)),
      logFile_(std::move(other.logFile_)) {
}

// Move assignment
DetectionLogger& DetectionLogger::operator=(DetectionLogger&& other) noexcept {
    if (this != &other) {
        if (logFile_.is_open()) {
            logFile_.close();
        }
        logFilePath_ = std::move(other.logFilePath_);
        logFile_ = std::move(other.logFile_);
    }
    return *this;
}

// Log successful detection
void DetectionLogger::LogSuccess(const LicenseResult& result) {
    std::ostringstream message;
    message << "SUCCESS - License Status: " << LicenseStatusToString(result.GetLicenseStatus())
            << ", Windows Version: " << result.GetWindowsVersion()
            << " (" << result.GetWindowsEdition() << ")"
            << ", KMS Status: " << KMSStatusToString(result.GetKmsStatus());

    if (!result.GetWindowsKmsServer().empty()) {
        message << ", Windows KMS Server: " << result.GetWindowsKmsServer();
    }
    if (!result.GetOfficeKmsServer().empty()) {
        message << ", Office KMS Server: " << result.GetOfficeKmsServer();
    }

    WriteLogEntry("INFO", message.str());
}

// Log detection failure
void DetectionLogger::LogFailure(const std::string& errorMessage) {
    WriteLogEntry("WARNING", "Detection failed: " + errorMessage);
}

// Log error exception
void DetectionLogger::LogError(const std::string& exceptionMessage) {
    WriteLogEntry("ERROR", "Exception: " + exceptionMessage);
}

// Log an informational message
void DetectionLogger::LogInfo(const std::string& message) {
    WriteLogEntry("INFO", message);
}

// Helper: Write formatted log entry with thread safety
void DetectionLogger::WriteLogEntry(const std::string& level, const std::string& message) {
    std::lock_guard<std::mutex> lock(logMutex_);

    if (!logFile_.is_open()) {
        // Reopen if file was closed
        logFile_.open(logFilePath_, std::ios::app);
    }

    if (logFile_.is_open()) {
        std::string timestamp = GetCurrentTimestamp();
        logFile_ << "[" << timestamp << "] [" << level << "] " << message << std::endl;
        logFile_.flush();  // Ensure immediate write
    }
}

// Helper: Get current timestamp
std::string DetectionLogger::GetCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);

    // Get milliseconds
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ) % 1000;

    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S")
        << "." << std::setfill('0') << std::setw(3) << ms.count();

    return oss.str();
}
