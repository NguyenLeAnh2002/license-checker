#include "NamedPipeClient.h"
#include <iostream>
#include <chrono>
#include <thread>

NamedPipeClient::NamedPipeClient(const std::string& pipeName)
    : m_pipeName(pipeName),
      m_pipeHandle(INVALID_HANDLE_VALUE),
      m_isConnected(false),
      m_isListening(false),
      m_stopListening(false)
{
}

NamedPipeClient::~NamedPipeClient()
{
    Disconnect();
    StopListening();
}

bool NamedPipeClient::Connect(int maxRetries, int retryDelayMs)
{
    std::string fullPipeName = "\\\\.\\pipe\\" + m_pipeName;

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
            // Set pipe mode to message mode
            DWORD mode = PIPE_READMODE_MESSAGE;
            if (SetNamedPipeHandleState(m_pipeHandle, &mode, nullptr, nullptr)) {
                m_isConnected = true;
                return true;
            }

            CloseHandle(m_pipeHandle);
            m_pipeHandle = INVALID_HANDLE_VALUE;
        }

        if (retry < maxRetries - 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(retryDelayMs));
        }
    }

    return false;
}

void NamedPipeClient::Disconnect()
{
    StopListening();

    if (m_pipeHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_pipeHandle);
        m_pipeHandle = INVALID_HANDLE_VALUE;
    }

    m_isConnected = false;
}

bool NamedPipeClient::IsConnected() const
{
    return m_isConnected && m_pipeHandle != INVALID_HANDLE_VALUE;
}

std::string NamedPipeClient::SendCommand(const std::string& command, const std::string& payload)
{
    if (!IsConnected()) {
        return "{\"status\":\"error\",\"error\":\"Not connected to service\"}";
    }

    std::string fullCommand = payload.empty() ? command : command + ":" + payload;

    if (!WriteCommand(fullCommand)) {
        return "{\"status\":\"error\",\"error\":\"Failed to send command\"}";
    }

    return ReadResponse(5000);
}

bool NamedPipeClient::SendCommandAsync(const std::string& command, const std::string& payload)
{
    if (!IsConnected()) {
        return false;
    }

    std::string fullCommand = payload.empty() ? command : command + ":" + payload;
    return WriteCommand(fullCommand);
}

void NamedPipeClient::SetDataReceivedCallback(DataReceivedCallback callback)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_dataReceivedCallback = callback;
}

void NamedPipeClient::StartListening()
{
    if (m_isListening) {
        return;
    }

    m_stopListening = false;
    m_isListening = true;
    m_listenThread = std::thread(&NamedPipeClient::ListenThread, this);
}

void NamedPipeClient::StopListening()
{
    if (!m_isListening) {
        return;
    }

    m_stopListening = true;

    if (m_listenThread.joinable()) {
        m_listenThread.join();
    }

    m_isListening = false;
}

void NamedPipeClient::ListenThread()
{
    while (!m_stopListening && IsConnected()) {
        std::string data = ReadResponse(1000);

        if (!data.empty()) {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_dataReceivedCallback) {
                m_dataReceivedCallback(data);
            }
        }
    }
}

std::string NamedPipeClient::ReadResponse(int timeoutMs)
{
    if (!IsConnected()) {
        return "";
    }

    char buffer[4096] = {0};
    DWORD bytesRead = 0;

    // Set read timeout
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = timeoutMs;
    timeouts.ReadTotalTimeoutConstant = timeoutMs;
    timeouts.ReadTotalTimeoutMultiplier = 0;

    if (ReadFile(m_pipeHandle, buffer, sizeof(buffer) - 1, &bytesRead, nullptr)) {
        return std::string(buffer, bytesRead);
    }

    return "";
}

bool NamedPipeClient::WriteCommand(const std::string& command)
{
    if (!IsConnected()) {
        return false;
    }

    DWORD bytesWritten = 0;

    if (WriteFile(m_pipeHandle, command.c_str(), static_cast<DWORD>(command.length()), &bytesWritten, nullptr)) {
        return bytesWritten == command.length();
    }

    return false;
}
