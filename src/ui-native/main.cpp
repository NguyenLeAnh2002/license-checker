#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "MainWindow.h"
#include "StartupLog.h"
#include <CommCtrl.h>
#include <objidl.h>
#include <gdiplus.h>
#include <sstream>

#pragma comment(lib, "gdiplus.lib")

// Global instance
HINSTANCE g_hInstance = NULL;

namespace {

// Last-resort backstop for crashes (access violation, stack overflow, etc.)
// that would otherwise leave a machine's "why didn't this report?" question
// completely unanswered - there's no other log for failures this early.
// EXCEPTION_EXECUTE_HANDLER suppresses the default Windows "stopped working"
// dialog, which would otherwise sit there forever on an unattended machine.
LONG WINAPI TopLevelCrashHandler(EXCEPTION_POINTERS* info) {
    std::ostringstream oss;
    oss << "FATAL: unhandled exception, code=0x" << std::hex
        << (info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionCode : 0)
        << " at address=0x"
        << (info && info->ExceptionRecord ? (void*)info->ExceptionRecord->ExceptionAddress : nullptr);
    StartupLog::Write(oss.str());
    return EXCEPTION_EXECUTE_HANDLER;
}

} // namespace

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    StartupLog::Write("=== LicenseCheckerUI process started ===");
    SetUnhandledExceptionFilter(TopLevelCrashHandler);

    g_hInstance = hInstance;

    try {
        // This is a Windows-subsystem app with no console of its own, but the
        // license detectors shell out to cscript.exe via _popen() (see
        // OfficeOSPPDetector/WindowsSLMgrDetector). Without a console to attach
        // to, each of those spawns (and briefly flashes) a brand new console
        // window. Allocating one here up front and immediately hiding it gives
        // _popen()'d children something to attach to instead.
        if (AllocConsole()) {
            HWND consoleWnd = GetConsoleWindow();
            if (consoleWnd) {
                ShowWindow(consoleWnd, SW_HIDE);
            }
        } else {
            StartupLog::Write("AllocConsole failed (error " + std::to_string(GetLastError()) + ") - continuing anyway");
        }

        // Initialize common controls
        INITCOMMONCONTROLSEX icc;
        icc.dwSize = sizeof(icc);
        icc.dwICC = ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES;
        InitCommonControlsEx(&icc);

        // GDI+ is used for the owner-drawn license status badges (MainWindow's
        // DrawStatusLabel) - must be started before any window is created and
        // shut down only after the message loop exits.
        ULONG_PTR gdiplusToken = 0;
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        Gdiplus::Status gdiStatus = Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
        if (gdiStatus != Gdiplus::Ok) {
            StartupLog::Write("GdiplusStartup failed, status=" + std::to_string((int)gdiStatus));
        }

        // Create and show main window
        MainWindow* pMainWindow = new MainWindow();
        if (!pMainWindow->Create()) {
            StartupLog::Write("FATAL: MainWindow::Create() failed (error " + std::to_string(GetLastError()) + ")");
            MessageBoxA(NULL, "Failed to create main window", "Error", MB_OK | MB_ICONERROR);
            delete pMainWindow;
            Gdiplus::GdiplusShutdown(gdiplusToken);
            return 1;
        }
        StartupLog::Write("Main window created, entering message loop");

        pMainWindow->Show(nCmdShow);

        // Message loop
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        StartupLog::Write("Message loop exited normally (exit code " + std::to_string((int)msg.wParam) + ")");

        delete pMainWindow;
        Gdiplus::GdiplusShutdown(gdiplusToken);
        return static_cast<int>(msg.wParam);
    } catch (const std::exception& ex) {
        StartupLog::Write(std::string("FATAL: unhandled exception in WinMain - ") + ex.what());
        return 1;
    } catch (...) {
        StartupLog::Write("FATAL: unknown unhandled exception in WinMain");
        return 1;
    }
}
