#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "MainWindow.h"
#include <CommCtrl.h>

// Global instance
HINSTANCE g_hInstance = NULL;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    g_hInstance = hInstance;

    // Initialize common controls
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icc);

    // Create and show main window
    MainWindow* pMainWindow = new MainWindow();
    if (!pMainWindow->Create()) {
        MessageBoxA(NULL, "Failed to create main window", "Error", MB_OK | MB_ICONERROR);
        delete pMainWindow;
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
    return static_cast<int>(msg.wParam);
}
