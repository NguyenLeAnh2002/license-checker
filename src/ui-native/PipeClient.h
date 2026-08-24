#pragma once

#include <windows.h>
#include <string>
#include <memory>

class PipeClient {
public:
    explicit PipeClient(const std::string& pipeName);
    ~PipeClient();

    bool Connect(int maxRetries = 5, int retryDelayMs = 500);
    void Disconnect();
    bool IsConnected() const;
    bool EnsureConnected();  // Reconnect if needed

    std::string SendCommand(const std::string& command, const std::string& payload = "");

private:
    bool WriteCommand(const std::string& command);
    std::string ReadResponse(int timeoutMs = 5000);

    std::string m_pipeName;
    HANDLE m_pipeHandle;
    bool m_isConnected;
};
