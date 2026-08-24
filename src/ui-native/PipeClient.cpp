#include "PipeClient.h"
#include <chrono>
#include <thread>
#include <fstream>
#include <sstream>
#include <iomanip>

static void LogMessage(const std::string& message) {
    const char* logFile = "license-detection.log";

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();

    std::ofstream out(logFile, std::ios::app);
    if (out.is_open()) {
        out << "[" << ss.str() << "] " << message << "\n";
        out.close();
    }
}

PipeClient::PipeClient(const std::string& pipeName)
    : m_pipeName(pipeName), m_pipeHandle(INVALID_HANDLE_VALUE), m_isConnected(false) {
}

PipeClient::~PipeClient() {
    Disconnect();
}

bool PipeClient::Connect(int maxRetries, int retryDelayMs) {
    std::string fullPipeName = "\\\\.\\pipe\\" + m_pipeName;
    LogMessage("UI: Attempting to connect to pipe '" + m_pipeName + "'");

    for (int retry = 0; retry < maxRetries; ++retry) {
        m_pipeHandle = CreateFileA(
            fullPipeName.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr
        );

        if (m_pipeHandle != INVALID_HANDLE_VALUE) {
            DWORD mode = PIPE_READMODE_MESSAGE;
            if (SetNamedPipeHandleState(m_pipeHandle, &mode, nullptr, nullptr)) {
                m_isConnected = true;
                LogMessage("UI: Successfully connected to pipe '" + m_pipeName + "'");
                return true;
            } else {
                DWORD error = GetLastError();
                LogMessage("UI: SetNamedPipeHandleState failed with error " + std::to_string(error));
                CloseHandle(m_pipeHandle);
                m_pipeHandle = INVALID_HANDLE_VALUE;
            }
        } else {
            DWORD error = GetLastError();
            LogMessage("UI: CreateFileA failed with error " + std::to_string(error) + " (retry " + std::to_string(retry + 1) + "/" + std::to_string(maxRetries) + ")");
        }

        if (retry < maxRetries - 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(retryDelayMs));
        }
    }

    LogMessage("UI: Failed to connect to pipe '" + m_pipeName + "' after " + std::to_string(maxRetries) + " retries");
    return false;
}

void PipeClient::Disconnect() {
    if (m_pipeHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_pipeHandle);
        m_pipeHandle = INVALID_HANDLE_VALUE;
        LogMessage("UI: Disconnected from pipe");
    }
    m_isConnected = false;
}

bool PipeClient::IsConnected() const {
    return m_isConnected && m_pipeHandle != INVALID_HANDLE_VALUE;
}

bool PipeClient::EnsureConnected() {
    if (IsConnected()) {
        return true;
    }
    LogMessage("UI: Connection lost, attempting to reconnect...");
    return Connect();
}

std::string PipeClient::SendCommand(const std::string& command, const std::string& payload) {
    // Try to connect if not already connected
    if (!EnsureConnected()) {
        LogMessage("UI: Error - Could not connect to service");
        return "{\"status\":\"error\",\"error\":\"Not connected to service\"}";
    }

    std::string fullCommand = payload.empty() ? command : command + ":" + payload;
    LogMessage("UI: Sending command - " + fullCommand);

    if (!WriteCommand(fullCommand)) {
        LogMessage("UI: Error - Failed to send command - " + fullCommand);
        Disconnect();  // Force reconnect on next attempt
        return "{\"status\":\"error\",\"error\":\"Failed to send command\"}";
    }

    std::string response = ReadResponse(5000);
    LogMessage("UI: Received response - " + response);
    return response;
}

bool PipeClient::WriteCommand(const std::string& command) {
    if (!IsConnected()) {
        return false;
    }

    DWORD bytesWritten = 0;

    if (WriteFile(m_pipeHandle, command.c_str(), static_cast<DWORD>(command.length()), &bytesWritten, nullptr)) {
        return bytesWritten == command.length();
    }

    return false;
}

std::string PipeClient::ReadResponse(int timeoutMs) {
    if (!IsConnected()) {
        return "";
    }

    char buffer[65536] = {0};  // Increased buffer size
    DWORD bytesRead = 0;

    if (ReadFile(m_pipeHandle, buffer, sizeof(buffer) - 1, &bytesRead, nullptr)) {
        return std::string(buffer, bytesRead);
    }

    return "";
}
