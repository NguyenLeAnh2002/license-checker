#include "NamedPipeServer.h"
#include "../license-detection/LicenseResult.h"
#include "../license-detection/LicenseStatusEnum.h"
#include <sstream>
#include <iostream>
#include <ctime>
#include <iomanip>
#include <fstream>
#include <chrono>

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

NamedPipeServer::NamedPipeServer(const std::string& pipeName)
    : m_pipeName(pipeName),
      m_pipeHandle(INVALID_HANDLE_VALUE),
      m_isRunning(false),
      m_stopRequested(false)
{
}

NamedPipeServer::~NamedPipeServer()
{
    Stop();
}

void NamedPipeServer::Start()
{
    if (m_isRunning) {
        return;
    }

    LogMessage("Service: NamedPipeServer starting on pipe '" + m_pipeName + "'");
    m_stopRequested = false;
    m_isRunning = true;
    m_listenThread = std::thread(&NamedPipeServer::ListenThread, this);
}

void NamedPipeServer::Stop()
{
    if (!m_isRunning) {
        return;
    }

    LogMessage("Service: NamedPipeServer stopping");
    m_stopRequested = true;

    // ListenThread blocks synchronously inside ConnectNamedPipe() while no
    // client is connected. Closing the handle from this thread does not
    // reliably unblock a pending synchronous ConnectNamedPipe() call on
    // Windows, which left this hanging indefinitely (service stuck in
    // STOP_PENDING). Connect a short-lived dummy client instead - that
    // satisfies the pending call, so ListenThread wakes up, sees
    // m_stopRequested, and exits its loop on its own.
    std::string fullPipeName = "\\\\.\\pipe\\" + m_pipeName;
    HANDLE hDummyClient = CreateFileA(
        fullPipeName.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0, nullptr, OPEN_EXISTING, 0, nullptr
    );
    if (hDummyClient != INVALID_HANDLE_VALUE) {
        CloseHandle(hDummyClient);
    }

    if (m_listenThread.joinable()) {
        m_listenThread.join();
    }

    if (m_pipeHandle != INVALID_HANDLE_VALUE) {
        DisconnectNamedPipe(m_pipeHandle);
        CloseHandle(m_pipeHandle);
        m_pipeHandle = INVALID_HANDLE_VALUE;
    }

    m_isRunning = false;
}

bool NamedPipeServer::IsRunning() const
{
    return m_isRunning;
}

void NamedPipeServer::SetRequestHandler(RequestHandler handler)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_requestHandler = handler;
}

void NamedPipeServer::SendNotification(const std::string& message)
{
    // In a real implementation, this would broadcast to all connected clients
    // For now, it's a placeholder for sending unsolicited notifications
}

void NamedPipeServer::ListenThread()
{
    std::string fullPipeName = "\\\\.\\pipe\\" + m_pipeName;
    int retryCount = 0;
    const int MAX_CREATE_RETRIES = 3;

    // Create security attributes to allow all users to access the pipe
    SECURITY_ATTRIBUTES sa;
    SECURITY_DESCRIPTOR sd;

    InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&sd, TRUE, nullptr, FALSE);

    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = &sd;
    sa.bInheritHandle = FALSE;

    while (!m_stopRequested) {
        // Create named pipe
        HANDLE hPipe = CreateNamedPipeA(
            fullPipeName.c_str(),
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,  // Allow multiple instances
            65536,  // Output buffer size (increased)
            65536,  // Input buffer size (increased)
            0,  // Default timeout
            &sa  // Allow all users to access
        );

        if (hPipe == INVALID_HANDLE_VALUE) {
            DWORD error = GetLastError();
            if (error == ERROR_PIPE_BUSY && retryCount < MAX_CREATE_RETRIES) {
                LogMessage("Service: Pipe busy, retrying... (attempt " + std::to_string(retryCount + 1) + ")");
                retryCount++;
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                continue;
            }
            LogMessage("Service: Failed to create named pipe (error: " + std::to_string(error) + ")");
            std::cerr << "Failed to create named pipe" << std::endl;
            break;
        }

        retryCount = 0;

        m_pipeHandle = hPipe;
        LogMessage("Service: Waiting for UI client connection on pipe '" + m_pipeName + "'");

        // Wait for client connection
        if (ConnectNamedPipe(hPipe, nullptr) || GetLastError() == ERROR_PIPE_CONNECTED) {
            LogMessage("Service: UI client connected");
            HandleClient(hPipe);
            LogMessage("Service: UI client disconnected");
        } else {
            DWORD error = GetLastError();
            LogMessage("Service: ConnectNamedPipe failed with error " + std::to_string(error));
        }

        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
        m_pipeHandle = INVALID_HANDLE_VALUE;

        // Small delay to allow client to reconnect
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void NamedPipeServer::HandleClient(HANDLE hPipe)
{
    while (!m_stopRequested) {
        std::string request = ReadFromPipe(hPipe);

        if (request.empty()) {
            break;  // Client disconnected
        }

        LogMessage("Service: Received command from UI - " + request);

        // Parse command (simple format: "command:payload")
        size_t colonPos = request.find(':');
        std::string command = (colonPos != std::string::npos) ? request.substr(0, colonPos) : request;
        std::string payload = (colonPos != std::string::npos) ? request.substr(colonPos + 1) : "";

        std::string response;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_requestHandler) {
                response = m_requestHandler(command, payload);
            } else {
                response = BuildErrorResponse("No handler registered");
            }
        }

        LogMessage("Service: Sending response to UI - " + response);

        if (!WriteToClient(hPipe, response)) {
            break;  // Failed to write, client disconnected
        }
    }
}

std::string NamedPipeServer::ReadFromPipe(HANDLE hPipe)
{
    char buffer[65536] = {0};  // Increased buffer size
    DWORD bytesRead = 0;

    if (ReadFile(hPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr)) {
        return std::string(buffer, bytesRead);
    }

    return "";  // Error or client disconnected
}

bool NamedPipeServer::WriteToClient(HANDLE hPipe, const std::string& data)
{
    DWORD bytesWritten = 0;

    if (WriteFile(hPipe, data.c_str(), static_cast<DWORD>(data.length()), &bytesWritten, nullptr)) {
        return true;
    }

    return false;
}

std::string NamedPipeServer::BuildLicenseDataResponse(const LicenseResult& result)
{
    std::ostringstream json;

    // Convert timestamp to ISO8601
    std::time_t timestamp = std::chrono::system_clock::to_time_t(result.GetTimestamp());
    struct tm timeinfo;
    localtime_s(&timeinfo, &timestamp);
    char timestampBuffer[30];
    strftime(timestampBuffer, sizeof(timestampBuffer), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);

    json << "{";
    json << "\"status\":\"success\",";
    json << "\"data\":{";

    // Detailed Windows License Information
    const auto& winInfo = result.GetWindowsLicenseInfo();
    json << "\"windows\":{";
    json << "\"version\":" << result.GetWindowsVersion() << ",";
    json << "\"edition\":\"" << result.GetWindowsEdition() << "\",";
    json << "\"licenseStatus\":" << static_cast<int>(result.GetLicenseStatus()) << ",";
    json << "\"kmsStatus\":" << static_cast<int>(result.GetKmsStatus()) << ",";
    json << "\"kmsServer\":\"" << result.GetWindowsKmsServer() << "\",";
    json << "\"lastDetected\":\"" << timestampBuffer << "\",";
    json << "\"name\":\"" << winInfo.name << "\",";
    json << "\"description\":\"" << winInfo.description << "\",";
    json << "\"licenseStatusDetail\":\"" << winInfo.licenseStatus << "\",";
    json << "\"partialProductKey\":\"" << winInfo.partialProductKey << "\"";
    json << "},";

    // Detailed Office License Information
    const auto& officeInfo = result.GetOfficeLicenseInfo();
    json << "\"office\":{";
    json << "\"kmsServer\":\"" << result.GetOfficeKmsServer() << "\",";
    json << "\"productId\":\"" << officeInfo.productId << "\",";
    json << "\"licenseName\":\"" << officeInfo.licenseName << "\",";
    json << "\"licenseStatus\":\"" << officeInfo.licenseStatus << "\",";
    json << "\"remainingGrace\":\"" << officeInfo.remainingGrace << "\",";
    json << "\"partialProductKey\":\"" << officeInfo.partialProductKey << "\",";
    json << "\"activationInterval\":\"" << officeInfo.activationInterval << "\"";
    json << "}";

    json << "}";
    json << "}";

    return json.str();
}

std::string NamedPipeServer::BuildErrorResponse(const std::string& error)
{
    std::ostringstream json;
    json << "{\"status\":\"error\",\"error\":\"" << error << "\"}";
    return json.str();
}

std::string NamedPipeServer::BuildSuccessResponse(const std::string& message)
{
    std::ostringstream json;
    json << "{\"status\":\"success\",\"message\":\"" << message << "\"}";
    return json.str();
}
