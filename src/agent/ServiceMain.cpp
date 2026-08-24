#include <windows.h>
#include <iostream>
#include <string>
#include <memory>
#include "LicenseDetectionWorker.h"

// Global service variables
static SERVICE_STATUS gServiceStatus = { 0 };
static SERVICE_STATUS_HANDLE gServiceStatusHandle = NULL;
static LicenseDetectionWorker* gpWorker = NULL;

// Forward declarations
void WINAPI ServiceMain(DWORD argc, LPTSTR* argv);
void WINAPI ServiceCtrlHandler(DWORD dwCtrl);
void ReportServiceStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint);

// Main entry point for Windows Service
void WINAPI ServiceMain(DWORD argc, LPTSTR* argv) {
    // Register the service control handler
    gServiceStatusHandle = RegisterServiceCtrlHandler(
        TEXT("LicenseCheckerAgent"),
        ServiceCtrlHandler
    );

    if (!gServiceStatusHandle) {
        return;
    }

    // Initialize service status
    gServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    gServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    gServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    gServiceStatus.dwWin32ExitCode = 0;
    gServiceStatus.dwServiceSpecificExitCode = 0;
    gServiceStatus.dwCheckPoint = 0;
    gServiceStatus.dwWaitHint = 0;

    // Report that the service is starting
    ReportServiceStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

    try {
        // Create and start the license detection worker
        gpWorker = new LicenseDetectionWorker();

        // Start license detection in background
        gpWorker->Start();

        // Report that the service is running
        ReportServiceStatus(SERVICE_RUNNING, NO_ERROR, 0);

        // Service is now running - wait here until stop is signaled
        // The service control handler will set appropriate status on stop
        while (gServiceStatus.dwCurrentState == SERVICE_RUNNING) {
            Sleep(1000);  // Check status every second
        }

        // Service is stopping - stop the worker
        if (gpWorker) {
            gpWorker->Stop();
            delete gpWorker;
            gpWorker = NULL;
        }

        // Report that the service has stopped
        ReportServiceStatus(SERVICE_STOPPED, NO_ERROR, 0);
    }
    catch (const std::exception& ex) {
        // Error during service operation
        ReportServiceStatus(SERVICE_STOPPED, ERROR_SERVICE_SPECIFIC_ERROR, 0);

        if (gpWorker) {
            delete gpWorker;
            gpWorker = NULL;
        }
    }
    catch (...) {
        // Unknown error
        ReportServiceStatus(SERVICE_STOPPED, ERROR_SERVICE_SPECIFIC_ERROR, 0);

        if (gpWorker) {
            delete gpWorker;
            gpWorker = NULL;
        }
    }
}

// Service control handler
void WINAPI ServiceCtrlHandler(DWORD dwCtrl) {
    switch (dwCtrl) {
        case SERVICE_CONTROL_STOP:
        case SERVICE_CONTROL_SHUTDOWN:
            // Request to stop the service
            gServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
            ReportServiceStatus(SERVICE_STOP_PENDING, NO_ERROR, 3000);

            // The main service loop will detect this and stop the worker
            break;

        case SERVICE_CONTROL_PAUSE:
            // Pause is not supported for this service
            break;

        case SERVICE_CONTROL_CONTINUE:
            // Continue is not supported for this service
            break;

        case SERVICE_CONTROL_INTERROGATE:
            // Report current status
            ReportServiceStatus(gServiceStatus.dwCurrentState, NO_ERROR, 0);
            break;

        default:
            // Unknown control code
            break;
    }
}

// Report service status to Windows Service Control Manager
void ReportServiceStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint) {
    static DWORD dwCheckPoint = 1;

    gServiceStatus.dwCurrentState = dwCurrentState;
    gServiceStatus.dwWin32ExitCode = dwWin32ExitCode;
    gServiceStatus.dwWaitHint = dwWaitHint;

    if (dwCurrentState == SERVICE_START_PENDING ||
        dwCurrentState == SERVICE_STOP_PENDING) {
        gServiceStatus.dwCheckPoint = dwCheckPoint++;
    } else {
        gServiceStatus.dwCheckPoint = 0;
    }

    SetServiceStatus(gServiceStatusHandle, &gServiceStatus);
}

// Entry point for the application (when run as a service)
int main(int argc, char* argv[]) {
    if (argc > 1 && (std::string(argv[1]) == "/test" || std::string(argv[1]) == "test")) {
        std::cout << "[TEST] Testing pipe connection..." << std::endl;

        try {
            // Create a named pipe client to test connection
            std::string pipeName = "\\\\.\\pipe\\LicenseChecker";

            HANDLE hPipe = CreateFileA(
                pipeName.c_str(),
                GENERIC_READ | GENERIC_WRITE,
                0,
                nullptr,
                OPEN_EXISTING,
                0,
                nullptr
            );

            if (hPipe != INVALID_HANDLE_VALUE) {
                std::cout << "[TEST]  Successfully connected to pipe: " << pipeName << std::endl;

                // Try to send GetLicenseData command
                std::string command = "GetLicenseData";
                DWORD bytesWritten = 0;

                if (WriteFile(hPipe, command.c_str(), static_cast<DWORD>(command.length()), &bytesWritten, nullptr)) {
                    std::cout << "[TEST] Sent command: " << command << std::endl;

                    // Read response
                    char buffer[4096] = { 0 };
                    DWORD bytesRead = 0;

                    if (ReadFile(hPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr)) {
                        std::cout << "[TEST]  Received response (" << bytesRead << " bytes):" << std::endl;
                        std::cout << buffer << std::endl;
                    }
                    else {
                        std::cout << "[TEST]  Failed to read response" << std::endl;
                    }
                }
                else {
                    std::cout << "[TEST]  Failed to send command" << std::endl;
                }

                CloseHandle(hPipe);
                return 0;
            }
            else {
                std::cout << "[TEST]  Failed to connect to pipe: " << pipeName << std::endl;
                std::cout << "[TEST] Error code: " << GetLastError() << std::endl;
                std::cout << "[TEST] Make sure service is running first!" << std::endl;
                return 1;
            }
        }
        catch (const std::exception& ex) {
            std::cerr << "[TEST] Error: " << ex.what() << std::endl;
            return 1;
        }
    }
    // Service dispatch table
    SERVICE_TABLE_ENTRY ServiceTable[] = {
        { TEXT("LicenseCheckerAgent"), ServiceMain },
        { NULL, NULL }
    };

    // Try to start the service control dispatcher
    if (!StartServiceCtrlDispatcher(ServiceTable)) {
        DWORD error = GetLastError();

        // If not running as a service, this will fail with ERROR_FAILED_SERVICE_CONTROLLER_CONNECT
        // In that case, we can run the worker directly for testing/debugging
        if (error == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
            // Run as console application for testing/debugging
            try {
                LicenseDetectionWorker worker;
                worker.Start();

                std::cout << "License Detection Worker started. Press Enter to stop..." << std::endl;
                std::cin.get();

                worker.Stop();
            }
            catch (const std::exception& ex) {
                std::cerr << "Error: " << ex.what() << std::endl;
                return 1;
            }

            return 0;
        }

        return 1;
    }

    return 0;
}
