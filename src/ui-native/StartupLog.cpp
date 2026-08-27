#include "StartupLog.h"
#include <windows.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <mutex>

namespace {

std::string GetExecutableDirectory() {
    char pathBuf[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, pathBuf, MAX_PATH);
    if (len == 0 || len == MAX_PATH) {
        return "";
    }
    std::string path(pathBuf, len);
    size_t pos = path.find_last_of("\\/");
    if (pos == std::string::npos) {
        return "";
    }
    return path.substr(0, pos);
}

std::string CurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::tm tmBuf;
    localtime_s(&tmBuf, &t);
    std::ostringstream oss;
    oss << std::put_time(&tmBuf, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

std::mutex g_logMutex;

} // namespace

void StartupLog::Write(const std::string& message) {
    std::lock_guard<std::mutex> lock(g_logMutex);

    std::string dir = GetExecutableDirectory();
    std::string path = (dir.empty() ? std::string() : dir + "\\") + "license_checker_startup.log";

    std::ofstream file(path, std::ios::app);
    if (file.is_open()) {
        file << "[" << CurrentTimestamp() << "] " << message << "\n";
    }
    // If it couldn't even open (e.g. read-only install folder), there's
    // nowhere else appropriate to report that - silently drop it, same as
    // DetectionLogger already does.
}
