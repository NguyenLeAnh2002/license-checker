#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "MainWindow.h"
#include <CommCtrl.h>
#include <objidl.h>
#include <gdiplus.h>

#pragma comment(lib, "gdiplus.lib")

// Global instance
HINSTANCE g_hInstance = NULL;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    g_hInstance = hInstance;

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
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    // Create and show main window
    MainWindow* pMainWindow = new MainWindow();
    if (!pMainWindow->Create()) {
        MessageBoxA(NULL, "Failed to create main window", "Error", MB_OK | MB_ICONERROR);
        delete pMainWindow;
        Gdiplus::GdiplusShutdown(gdiplusToken);
        return 1;
    }

    pMainWindow->Show(nCmdShow);

    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    delete pMainWindow;
    Gdiplus::GdiplusShutdown(gdiplusToken);
    return static_cast<int>(msg.wParam);
}
