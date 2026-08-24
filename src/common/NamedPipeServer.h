#ifndef NAMEDPIPESERVER_H
#define NAMEDPIPESERVER_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN

#include <string>
#include <thread>
#include <mutex>
#include <functional>

class LicenseResult;

class NamedPipeServer {
public:
    using RequestHandler = std::function<std::string(const std::string& command, const std::string& payload)>;

    explicit NamedPipeServer(const std::string& pipeName = "LicenseChecker");
    ~NamedPipeServer();

    void Start();
    void Stop();
    bool IsRunning() const;

    void SetRequestHandler(RequestHandler handler);
    void SendNotification(const std::string& message);

    // Helpers to build JSON responses
    static std::string BuildLicenseDataResponse(const LicenseResult& result);
    static std::string BuildErrorResponse(const std::string& error);
    static std::string BuildSuccessResponse(const std::string& message);

private:
    void ListenThread();
    void HandleClient(HANDLE hPipe);
    std::string ReadFromPipe(HANDLE hPipe);
    bool WriteToClient(HANDLE hPipe, const std::string& data);

    std::string m_pipeName;
    HANDLE m_pipeHandle;
    std::thread m_listenThread;
    std::mutex m_mutex;
    bool m_isRunning;
    bool m_stopRequested;
    RequestHandler m_requestHandler;
};

#endif // NAMEDPIPESERVER_H
