#ifndef NAMEDPIPECLIENT_H
#define NAMEDPIPECLIENT_H

#include <string>
#include <windows.h>
#include <functional>
#include <thread>
#include <mutex>

class NamedPipeClient {
public:
    using DataReceivedCallback = std::function<void(const std::string& data)>;

    explicit NamedPipeClient(const std::string& pipeName = "LicenseChecker");
    ~NamedPipeClient();

    bool Connect(int maxRetries = 5, int retryDelayMs = 1000);
    void Disconnect();
    bool IsConnected() const;

    // Send command and wait for response
    std::string SendCommand(const std::string& command, const std::string& payload = "");

    // Send command without waiting for response (async)
    bool SendCommandAsync(const std::string& command, const std::string& payload = "");

    void SetDataReceivedCallback(DataReceivedCallback callback);
    void StartListening();
    void StopListening();

private:
    void ListenThread();
    std::string ReadResponse(int timeoutMs = 5000);
    bool WriteCommand(const std::string& command);

    std::string m_pipeName;
    HANDLE m_pipeHandle;
    std::thread m_listenThread;
    std::mutex m_mutex;
    bool m_isConnected;
    bool m_isListening;
    bool m_stopListening;
    DataReceivedCallback m_dataReceivedCallback;
};

#endif // NAMEDPIPECLIENT_H
