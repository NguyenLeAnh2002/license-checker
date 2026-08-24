#include "ServiceInstaller.h"
#include <windows.h>
#include <winsvc.h>
#include <iostream>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "kernel32.lib")

// Install the License Checker service
bool InstallService(const std::string& serviceName, const std::string& displayName, const std::string& exePath) {
    // Open the Service Control Manager
    SC_HANDLE schSCManager = OpenSCManager(
        NULL,                   // Local machine
        NULL,                   // Service database
        SC_MANAGER_CREATE_SERVICE  // Required access
    );

    if (!schSCManager) {
        std::cerr << "Failed to open Service Control Manager. Error: " << GetLastError() << std::endl;
        return false;
    }

    // Create the service
    SC_HANDLE schService = CreateServiceA(
        schSCManager,                           // Service manager handle
        serviceName.c_str(),                    // Service name
        displayName.c_str(),                    // Display name
        SERVICE_ALL_ACCESS,                     // Desired access
        SERVICE_WIN32_OWN_PROCESS,              // Service type
        SERVICE_AUTO_START,                     // Start type (auto-start)
        SERVICE_ERROR_NORMAL,                   // Error control
        exePath.c_str(),                        // Binary path
        NULL,                                   // Load order group
        NULL,                                   // Tag ID
        NULL,                                   // Dependencies
        NULL,                                   // Service account (LocalSystem)
        NULL                                    // Password
    );

    if (!schService) {
        DWORD error = GetLastError();
        if (error == ERROR_SERVICE_EXISTS) {
            // Reinstall case: wipe the old registration and recreate it fresh
            // rather than reusing whatever was registered on a previous
            // install, so the service config always matches this run exactly.
            std::cout << "Service already exists - removing old registration and recreating it." << std::endl;
            CloseServiceHandle(schSCManager);

            if (!UninstallService(serviceName)) {
                std::cerr << "Failed to remove the existing service registration; cannot recreate it." << std::endl;
                return false;
            }

            schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
            if (!schSCManager) {
                std::cerr << "Failed to open Service Control Manager. Error: " << GetLastError() << std::endl;
                return false;
            }

            schService = CreateServiceA(
                schSCManager, serviceName.c_str(), displayName.c_str(),
                SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START,
                SERVICE_ERROR_NORMAL, exePath.c_str(),
                NULL, NULL, NULL, NULL, NULL
            );

            if (!schService) {
                std::cerr << "Failed to recreate service after removing old registration. Error: "
                          << GetLastError() << std::endl;
                CloseServiceHandle(schSCManager);
                return false;
            }
        } else {
            std::cerr << "Failed to create service. Error: " << error << std::endl;
            CloseServiceHandle(schSCManager);
            return false;
        }
    }

    std::cout << "Service '" << displayName << "' installed successfully." << std::endl;

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
    return true;
}

// Uninstall the License Checker service
bool UninstallService(const std::string& serviceName) {
    // Open the Service Control Manager
    SC_HANDLE schSCManager = OpenSCManager(
        NULL,
        NULL,
        SC_MANAGER_ALL_ACCESS
    );

    if (!schSCManager) {
        std::cerr << "Failed to open Service Control Manager. Error: " << GetLastError() << std::endl;
        return false;
    }

    // Open the service
    SC_HANDLE schService = OpenServiceA(
        schSCManager,
        serviceName.c_str(),
        DELETE | SERVICE_STOP | SERVICE_QUERY_STATUS
    );

    if (!schService) {
        std::cerr << "Failed to open service. Error: " << GetLastError() << std::endl;
        CloseServiceHandle(schSCManager);
        return false;
    }

    // Stop the service first
    SERVICE_STATUS status = { 0 };
    ControlService(schService, SERVICE_CONTROL_STOP, &status);

    // Wait a moment for the service to stop
    Sleep(1000);

    // Delete the service
    if (!DeleteService(schService)) {
        std::cerr << "Failed to delete service. Error: " << GetLastError() << std::endl;
        CloseServiceHandle(schService);
        CloseServiceHandle(schSCManager);
        return false;
    }

    std::cout << "Service uninstalled successfully." << std::endl;

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
    return true;
}

// Start the service
bool StartServiceNow(const std::string& serviceName) {
    SC_HANDLE schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!schSCManager) {
        std::cerr << "Failed to open Service Control Manager. Error: " << GetLastError() << std::endl;
        return false;
    }

    SC_HANDLE schService = OpenServiceA(schSCManager, serviceName.c_str(), SERVICE_START);
    if (!schService) {
        std::cerr << "Failed to open service. Error: " << GetLastError() << std::endl;
        CloseServiceHandle(schSCManager);
        return false;
    }

    if (!StartServiceA(schService, 0, NULL)) {
        std::cerr << "Failed to start service. Error: " << GetLastError() << std::endl;
        CloseServiceHandle(schService);
        CloseServiceHandle(schSCManager);
        return false;
    }

    std::cout << "Service started successfully." << std::endl;

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
    return true;
}

// Stop the service
bool StopServiceNow(const std::string& serviceName) {
    SC_HANDLE schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!schSCManager) {
        std::cerr << "Failed to open Service Control Manager. Error: " << GetLastError() << std::endl;
        return false;
    }

    SC_HANDLE schService = OpenServiceA(schSCManager, serviceName.c_str(), SERVICE_STOP);
    if (!schService) {
        std::cerr << "Failed to open service. Error: " << GetLastError() << std::endl;
        CloseServiceHandle(schSCManager);
        return false;
    }

    SERVICE_STATUS status = { 0 };
    if (!ControlService(schService, SERVICE_CONTROL_STOP, &status)) {
        std::cerr << "Failed to stop service. Error: " << GetLastError() << std::endl;
        CloseServiceHandle(schService);
        CloseServiceHandle(schSCManager);
        return false;
    }

    std::cout << "Service stopped successfully." << std::endl;

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
    return true;
}
