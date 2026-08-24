#include "MainWindow.h"
#include "Localization.h"
#include "PipeClient.h"
#include "resource.h"
#include <string>
#include <commctrl.h>
#include <shlobj.h>
#include <windowsx.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <winreg.h>
#include <D:\\VNPT\\packages\\nlohmann.json.3.10.0\\build\\native\\include\\nlohmann\\json.hpp>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "advapi32.lib")

extern HINSTANCE g_hInstance;

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

const int ID_TAB_CONTROL = 1001;
const int ID_STATUS_LABEL = 1002;
const int ID_CHECK_BUTTON = 1003;
const int ID_REFRESH_LABEL = 1004;
const int ID_LANGUAGE_COMBO = 1005;

const int TAB_WINDOWS = 0;
const int TAB_OFFICE = 1;
const int TAB_SETTINGS = 2;

MainWindow::MainWindow()
    : m_hwnd(NULL), m_hTabControl(NULL), m_hCheckButton(NULL),
      m_hRefreshLabel(NULL), m_hLanguageLabel(NULL), m_hLanguageCombo(NULL),
      m_hWindowsStatusLabel(NULL), m_hWindowsNameLabel(NULL), m_hWindowsEditionLabel(NULL),
      m_hWindowsDescriptionLabel(NULL), m_hWindowsLicenseDetailLabel(NULL),
      m_hWindowsProductKeyLabel(NULL), m_hWindowsKmsStatusLabel(NULL),
      m_hWindowsKmsLabel(NULL), m_hWindowsLastDetectedLabel(NULL),
      m_hOfficeStatusLabel(NULL), m_hOfficeLicenseNameLabel(NULL),
      m_hOfficeLicenseStatusLabel(NULL), m_hOfficeProductKeyLabel(NULL),
      m_hOfficeKmsLabel(NULL), m_hOfficeGracePeriodLabel(NULL),
      m_hOfficeActivationIntervalLabel(NULL),
      m_hHostnameLabel(NULL), m_hMachineGuidLabel(NULL), m_hDepartmentLabel(NULL),
      m_hServiceStatusLabel(NULL), m_hLastReportLabel(NULL),
      m_pipeClient(std::make_unique<PipeClient>("LicenseChecker")) {
}

MainWindow::~MainWindow() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
    }
}

bool MainWindow::Create() {
    // Register window class
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = g_hInstance;
    wc.lpszClassName = L"LicenseCheckerWindow";
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hIcon = LoadIconW(g_hInstance, MAKEINTRESOURCEW(IDI_APPICON));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    if (!RegisterClass(&wc)) {
        return false;
    }

    // Create main window
    m_hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        L"LicenseCheckerWindow",
        Localization::T(Str::WindowTitle),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        800, 400,
        NULL, NULL, g_hInstance, this
    );

    if (!m_hwnd) {
        return false;
    }

    CreateControls();
    RefreshLicenseData();

    // Start auto-refresh timer (5 minutes = 300000ms)
    SetTimer(m_hwnd, 1, 300000, NULL);

    return true;
}

void MainWindow::Show(int nCmdShow) {
    if (m_hwnd) {
        ShowWindow(m_hwnd, nCmdShow);
        UpdateWindow(m_hwnd);
    }
}

LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MainWindow* pThis = NULL;

    if (msg == WM_CREATE) {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = reinterpret_cast<MainWindow*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
    } else {
        pThis = reinterpret_cast<MainWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (!pThis) {
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    switch (msg) {
    case WM_CLOSE:
        PostQuitMessage(0);
        return 0;

    case WM_SIZE:
        pThis->OnSize(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_COMMAND:
        pThis->OnCommand(wParam, lParam);
        return 0;

    case WM_NOTIFY:
        pThis->OnNotify(lParam);
        return 0;

    case WM_TIMER:
        pThis->RefreshLicenseData();
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

void MainWindow::CreateControls() {
    // Tab Control
    m_hTabControl = CreateWindowW(
        L"SysTabControl32", L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | TCS_TABS,
        10, 10, 780, 280,
        m_hwnd, (HMENU)ID_TAB_CONTROL, g_hInstance, NULL
    );

    // Insert 3 tabs: Windows License, Office License, Settings
    TCITEMW tie;
    tie.mask = TCIF_TEXT;

    // Tab 0: Windows License
    tie.pszText = const_cast<wchar_t*>(Localization::T(Str::TabWindows));
    SendMessageW(m_hTabControl, TCM_INSERTITEMW, TAB_WINDOWS, (LPARAM)&tie);

    // Tab 1: Office License
    tie.pszText = const_cast<wchar_t*>(Localization::T(Str::TabOffice));
    SendMessageW(m_hTabControl, TCM_INSERTITEMW, TAB_OFFICE, (LPARAM)&tie);

    // Tab 2: Settings
    tie.pszText = const_cast<wchar_t*>(Localization::T(Str::TabSettings));
    SendMessageW(m_hTabControl, TCM_INSERTITEMW, TAB_SETTINGS, (LPARAM)&tie);

    // Windows License content
    int windowsYPos = 50;
    int lineHeight = 22;

    m_hWindowsNameLabel = CreateWindowW(
        L"STATIC", L"Name: Unknown",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, windowsYPos, 730, lineHeight,
        m_hTabControl, NULL, g_hInstance, NULL
    );
    windowsYPos += lineHeight;

    m_hWindowsEditionLabel = CreateWindowW(
        L"STATIC", L"Edition: Unknown",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, windowsYPos, 730, lineHeight,
        m_hTabControl, NULL, g_hInstance, NULL
    );
    windowsYPos += lineHeight;

    m_hWindowsDescriptionLabel = CreateWindowW(
        L"STATIC", L"Description: Unknown",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, windowsYPos, 730, lineHeight,
        m_hTabControl, NULL, g_hInstance, NULL
    );
    windowsYPos += lineHeight;

    m_hWindowsLicenseDetailLabel = CreateWindowW(
        L"STATIC", L"License Status: Unknown",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, windowsYPos, 730, lineHeight,
        m_hTabControl, NULL, g_hInstance, NULL
    );
    windowsYPos += lineHeight;

    m_hWindowsProductKeyLabel = CreateWindowW(
        L"STATIC", L"Product Key: Unknown",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, windowsYPos, 730, lineHeight,
        m_hTabControl, NULL, g_hInstance, NULL
    );
    windowsYPos += lineHeight;

    m_hWindowsKmsStatusLabel = CreateWindowW(
        L"STATIC", L"KMS Status: Unknown",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, windowsYPos, 730, lineHeight,
        m_hTabControl, NULL, g_hInstance, NULL
    );
    windowsYPos += lineHeight;

    m_hWindowsKmsLabel = CreateWindowW(
        L"STATIC", L"KMS Server: Not Detected",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, windowsYPos, 730, lineHeight,
        m_hTabControl, NULL, g_hInstance, NULL
    );
    windowsYPos += lineHeight;

    m_hWindowsLastDetectedLabel = CreateWindowW(
        L"STATIC", L"Last Detected: Never",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, windowsYPos, 730, lineHeight,
        m_hTabControl, NULL, g_hInstance, NULL
    );

    // Office License content
    int officeYPos = 50;

    m_hOfficeStatusLabel = CreateWindowW(
        L"STATIC", L"Status: Unknown",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, officeYPos, 730, lineHeight,
        m_hTabControl, NULL, g_hInstance, NULL
    );
    officeYPos += lineHeight;

    m_hOfficeLicenseNameLabel = CreateWindowW(
        L"STATIC", L"License Name: Unknown",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, officeYPos, 730, lineHeight,
        m_hTabControl, NULL, g_hInstance, NULL
    );
    officeYPos += lineHeight;

    m_hOfficeLicenseStatusLabel = CreateWindowW(
        L"STATIC", L"License Status: Unknown",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, officeYPos, 730, lineHeight,
        m_hTabControl, NULL, g_hInstance, NULL
    );
    officeYPos += lineHeight;

    m_hOfficeProductKeyLabel = CreateWindowW(
        L"STATIC", L"Product Key: Unknown",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, officeYPos, 730, lineHeight,
        m_hTabControl, NULL, g_hInstance, NULL
    );
    officeYPos += lineHeight;

    m_hOfficeKmsLabel = CreateWindowW(
        L"STATIC", L"KMS Server: Not Detected",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, officeYPos, 730, lineHeight,
        m_hTabControl, NULL, g_hInstance, NULL
    );
    officeYPos += lineHeight;

    m_hOfficeGracePeriodLabel = CreateWindowW(
        L"STATIC", L"Grace Period: Unknown",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, officeYPos, 730, lineHeight,
        m_hTabControl, NULL, g_hInstance, NULL
    );
    officeYPos += lineHeight;

    m_hOfficeActivationIntervalLabel = CreateWindowW(
        L"STATIC", L"Activation Interval: Unknown",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, officeYPos, 730, lineHeight,
        m_hTabControl, NULL, g_hInstance, NULL
    );

    // Settings content
    int settingsYPos = 50;

    m_hHostnameLabel = CreateWindowW(
        L"STATIC", L"Hostname: Unknown",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, settingsYPos, 730, lineHeight,
        m_hTabControl, NULL, g_hInstance, NULL
    );
    settingsYPos += lineHeight;

    m_hMachineGuidLabel = CreateWindowW(
        L"STATIC", L"Machine GUID: Unknown",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, settingsYPos, 730, lineHeight,
        m_hTabControl, NULL, g_hInstance, NULL
    );
    settingsYPos += lineHeight;

    m_hDepartmentLabel = CreateWindowW(
        L"STATIC", L"Department: Unknown",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, settingsYPos, 730, lineHeight,
        m_hTabControl, NULL, g_hInstance, NULL
    );
    settingsYPos += lineHeight;

    m_hServiceStatusLabel = CreateWindowW(
        L"STATIC", L"Service Status: Offline",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, settingsYPos, 730, lineHeight,
        m_hTabControl, NULL, g_hInstance, NULL
    );
    settingsYPos += lineHeight;

    // Language selector in Settings tab (created as child of main window, not tab control)
    // This ensures WM_COMMAND messages are sent to m_hwnd's WndProc
    m_hLanguageLabel = CreateWindowW(
        L"STATIC", L"Language:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, settingsYPos, 100, lineHeight,
        m_hTabControl, NULL, g_hInstance, NULL
    );

    m_hLanguageCombo = CreateWindowW(
        L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
        140, settingsYPos, 120, 200,
        m_hwnd, (HMENU)ID_LANGUAGE_COMBO, g_hInstance, NULL
    );

    SendMessageW(m_hLanguageCombo, CB_ADDSTRING, 0, (LPARAM)L"Tiếng Việt");
    SendMessageW(m_hLanguageCombo, CB_ADDSTRING, 0, (LPARAM)L"English");
    SendMessageW(m_hLanguageCombo, CB_SETCURSEL, 0, 0);  // Default to Vietnamese

    // Bottom buttons (outside tabs)
    m_hRefreshLabel = CreateWindowW(
        L"STATIC", L"Ready",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        10, 300, 600, 20,
        m_hwnd, NULL, g_hInstance, NULL
    );

    m_hCheckButton = CreateWindowW(
        L"BUTTON", L"Check Now",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        680, 295, 90, 30,
        m_hwnd, (HMENU)ID_CHECK_BUTTON, g_hInstance, NULL
    );

    // Set font
    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    SendMessage(m_hTabControl, WM_SETFONT, (WPARAM)hFont, FALSE);
    SendMessage(m_hWindowsNameLabel, WM_SETFONT, (WPARAM)hFont, FALSE);
    SendMessage(m_hWindowsEditionLabel, WM_SETFONT, (WPARAM)hFont, FALSE);
    SendMessage(m_hWindowsDescriptionLabel, WM_SETFONT, (WPARAM)hFont, FALSE);
    SendMessage(m_hWindowsLicenseDetailLabel, WM_SETFONT, (WPARAM)hFont, FALSE);
    SendMessage(m_hWindowsProductKeyLabel, WM_SETFONT, (WPARAM)hFont, FALSE);
    SendMessage(m_hWindowsKmsStatusLabel, WM_SETFONT, (WPARAM)hFont, FALSE);
    SendMessage(m_hWindowsKmsLabel, WM_SETFONT, (WPARAM)hFont, FALSE);
    SendMessage(m_hWindowsLastDetectedLabel, WM_SETFONT, (WPARAM)hFont, FALSE);
    SendMessage(m_hOfficeStatusLabel, WM_SETFONT, (WPARAM)hFont, FALSE);
    SendMessage(m_hOfficeLicenseNameLabel, WM_SETFONT, (WPARAM)hFont, FALSE);
    SendMessage(m_hOfficeLicenseStatusLabel, WM_SETFONT, (WPARAM)hFont, FALSE);
    SendMessage(m_hOfficeProductKeyLabel, WM_SETFONT, (WPARAM)hFont, FALSE);
    SendMessage(m_hOfficeKmsLabel, WM_SETFONT, (WPARAM)hFont, FALSE);
    SendMessage(m_hOfficeGracePeriodLabel, WM_SETFONT, (WPARAM)hFont, FALSE);
    SendMessage(m_hOfficeActivationIntervalLabel, WM_SETFONT, (WPARAM)hFont, FALSE);
    SendMessage(m_hHostnameLabel, WM_SETFONT, (WPARAM)hFont, FALSE);
    SendMessage(m_hMachineGuidLabel, WM_SETFONT, (WPARAM)hFont, FALSE);
    SendMessage(m_hDepartmentLabel, WM_SETFONT, (WPARAM)hFont, FALSE);
    SendMessage(m_hServiceStatusLabel, WM_SETFONT, (WPARAM)hFont, FALSE);
    SendMessage(m_hRefreshLabel, WM_SETFONT, (WPARAM)hFont, FALSE);
    SendMessage(m_hCheckButton, WM_SETFONT, (WPARAM)hFont, FALSE);
    SendMessage(m_hLanguageLabel, WM_SETFONT, (WPARAM)hFont, FALSE);
    SendMessage(m_hLanguageCombo, WM_SETFONT, (WPARAM)hFont, FALSE);

    // Set initial tab visibility and update UI texts from localization
    SwitchTab(TAB_WINDOWS);
    UpdateUITexts();

    // Ensure tab control is on top of combo box (z-order fix)
    SetWindowPos(m_hTabControl, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

void MainWindow::OnSize(int cx, int cy) {
    // Resize controls based on window size
    if (m_hCheckButton) {
        MoveWindow(m_hCheckButton, cx - 100, cy - 50, 90, 30, TRUE);
    }
}

void MainWindow::OnCommand(WPARAM wParam, LPARAM lParam) {
    int wmId = LOWORD(wParam);
    int wmEvent = HIWORD(wParam);

    if (wmId == ID_CHECK_BUTTON) {
        CheckNow();
    } else if (wmId == ID_LANGUAGE_COMBO && wmEvent == CBN_SELCHANGE) {
        OnLanguageChanged();
    }
}

void MainWindow::UpdateChromeTexts() {
    // Update only UI chrome (button, status label, tab names, window title, language label)
    // Does NOT reset content labels (Name, Edition, etc.) - they keep their current data
    SetWindowTextW(m_hCheckButton, Localization::T(Str::CheckNow));
    SetWindowTextW(m_hRefreshLabel, Localization::T(Str::Ready));
    SetWindowTextW(m_hLanguageLabel, Localization::T(Str::LanguageLabel));

    // Update tab labels
    TCITEMW tie;
    tie.mask = TCIF_TEXT;
    tie.pszText = const_cast<wchar_t*>(Localization::T(Str::TabWindows));
    SendMessageW(m_hTabControl, TCM_SETITEMW, TAB_WINDOWS, (LPARAM)&tie);
    tie.pszText = const_cast<wchar_t*>(Localization::T(Str::TabOffice));
    SendMessageW(m_hTabControl, TCM_SETITEMW, TAB_OFFICE, (LPARAM)&tie);
    tie.pszText = const_cast<wchar_t*>(Localization::T(Str::TabSettings));
    SendMessageW(m_hTabControl, TCM_SETITEMW, TAB_SETTINGS, (LPARAM)&tie);

    // Update window title
    SetWindowTextW(m_hwnd, Localization::T(Str::WindowTitle));
}

void MainWindow::UpdateContentLabelsPrefix() {
    // Update only prefix of content labels, preserving current data
    LogMessage("UI: UpdateContentLabelsPrefix - Start");

    auto updateLabel = [this](HWND hLabel, Str prefixId) {
        if (!hLabel) {
            LogMessage("UI: UpdateContentLabelsPrefix - label is NULL");
            return;
        }
        wchar_t buffer[512];
        GetWindowTextW(hLabel, buffer, sizeof(buffer) / sizeof(wchar_t));
        std::wstring currentText(buffer);

        // Find ": " separator
        size_t pos = currentText.find(L": ");
        if (pos != std::wstring::npos) {
            std::wstring data = currentText.substr(pos + 2); // +2 to skip ": "
            std::wstring newText = Localization::T(prefixId);
            newText += L": ";
            newText += data;
            SetWindowTextW(hLabel, newText.c_str());
            // Narrow wstring to string for logging (ASCII values only)
            std::string narrowData(data.begin(), data.end());
            LogMessage("UI: UpdateContentLabelsPrefix - updated label, data='" + narrowData + "'");
        } else {
            std::string narrowText(currentText.begin(), currentText.end());
            LogMessage("UI: UpdateContentLabelsPrefix - no ': ' separator found in: " + narrowText);
        }
    };

    // Update Windows License labels
    updateLabel(m_hWindowsNameLabel, Str::Name);
    updateLabel(m_hWindowsEditionLabel, Str::Edition);
    updateLabel(m_hWindowsDescriptionLabel, Str::Description);
    updateLabel(m_hWindowsLicenseDetailLabel, Str::LicenseStatus);
    updateLabel(m_hWindowsProductKeyLabel, Str::ProductKey);
    updateLabel(m_hWindowsKmsStatusLabel, Str::KmsStatus);
    updateLabel(m_hWindowsKmsLabel, Str::KmsServer);
    updateLabel(m_hWindowsLastDetectedLabel, Str::LastDetected);

    // Update Office License labels
    updateLabel(m_hOfficeStatusLabel, Str::Status);
    updateLabel(m_hOfficeLicenseNameLabel, Str::LicenseName);
    updateLabel(m_hOfficeLicenseStatusLabel, Str::LicenseStatus);
    updateLabel(m_hOfficeProductKeyLabel, Str::ProductKey);
    updateLabel(m_hOfficeKmsLabel, Str::KmsServer);
    updateLabel(m_hOfficeGracePeriodLabel, Str::GracePeriod);
    updateLabel(m_hOfficeActivationIntervalLabel, Str::ActivationInterval);

    // Update Settings labels
    updateLabel(m_hHostnameLabel, Str::Hostname);
    updateLabel(m_hMachineGuidLabel, Str::MachineGuid);
    updateLabel(m_hDepartmentLabel, Str::Department);
    updateLabel(m_hServiceStatusLabel, Str::ServiceStatus);

    LogMessage("UI: UpdateContentLabelsPrefix - End");
}

void MainWindow::UpdateUITexts() {
    // Update chrome AND reset content labels to "Unknown" (used at startup)
    UpdateChromeTexts();

    SetWindowTextW(m_hCheckButton, Localization::T(Str::CheckNow));
    SetWindowTextW(m_hRefreshLabel, Localization::T(Str::Ready));
    SetWindowTextW(m_hWindowsNameLabel,
        (std::wstring(Localization::T(Str::Name)) + L" Unknown").c_str());
    SetWindowTextW(m_hWindowsEditionLabel,
        (std::wstring(Localization::T(Str::Edition)) + L" Unknown").c_str());
    SetWindowTextW(m_hWindowsDescriptionLabel,
        (std::wstring(Localization::T(Str::Description)) + L" Unknown").c_str());
    SetWindowTextW(m_hWindowsLicenseDetailLabel,
        (std::wstring(Localization::T(Str::LicenseStatus)) + L" Unknown").c_str());
    SetWindowTextW(m_hWindowsProductKeyLabel,
        (std::wstring(Localization::T(Str::ProductKey)) + L" Unknown").c_str());
    SetWindowTextW(m_hWindowsKmsStatusLabel,
        (std::wstring(Localization::T(Str::KmsStatus)) + L" Unknown").c_str());
    SetWindowTextW(m_hWindowsKmsLabel,
        (std::wstring(Localization::T(Str::KmsServer)) + L" ").c_str());
    SetWindowTextW(m_hWindowsLastDetectedLabel,
        (std::wstring(Localization::T(Str::LastDetected)) + L" ").c_str());

    SetWindowTextW(m_hOfficeStatusLabel,
        (std::wstring(Localization::T(Str::Status)) + L" Unknown").c_str());
    SetWindowTextW(m_hOfficeLicenseNameLabel,
        (std::wstring(Localization::T(Str::LicenseName)) + L" Unknown").c_str());
    SetWindowTextW(m_hOfficeLicenseStatusLabel,
        (std::wstring(Localization::T(Str::LicenseStatus)) + L" Unknown").c_str());
    SetWindowTextW(m_hOfficeProductKeyLabel,
        (std::wstring(Localization::T(Str::ProductKey)) + L" Unknown").c_str());
    SetWindowTextW(m_hOfficeKmsLabel,
        (std::wstring(Localization::T(Str::KmsServer)) + L" ").c_str());
    SetWindowTextW(m_hOfficeGracePeriodLabel,
        (std::wstring(Localization::T(Str::GracePeriod)) + L" ").c_str());
    SetWindowTextW(m_hOfficeActivationIntervalLabel,
        (std::wstring(Localization::T(Str::ActivationInterval)) + L" ").c_str());

    SetWindowTextW(m_hHostnameLabel,
        (std::wstring(Localization::T(Str::Hostname)) + L" Unknown").c_str());
    SetWindowTextW(m_hMachineGuidLabel,
        (std::wstring(Localization::T(Str::MachineGuid)) + L" Unknown").c_str());
    SetWindowTextW(m_hDepartmentLabel,
        (std::wstring(Localization::T(Str::Department)) + L" Unknown").c_str());
    SetWindowTextW(m_hServiceStatusLabel,
        (std::wstring(Localization::T(Str::ServiceStatus)) + L" ").c_str());

    SetWindowTextW(m_hLanguageLabel, Localization::T(Str::LanguageLabel));

    // Update tab labels
    TCITEMW tie;
    tie.mask = TCIF_TEXT;
    tie.pszText = const_cast<wchar_t*>(Localization::T(Str::TabWindows));
    SendMessageW(m_hTabControl, TCM_SETITEMW, TAB_WINDOWS, (LPARAM)&tie);
    tie.pszText = const_cast<wchar_t*>(Localization::T(Str::TabOffice));
    SendMessageW(m_hTabControl, TCM_SETITEMW, TAB_OFFICE, (LPARAM)&tie);
    tie.pszText = const_cast<wchar_t*>(Localization::T(Str::TabSettings));
    SendMessageW(m_hTabControl, TCM_SETITEMW, TAB_SETTINGS, (LPARAM)&tie);

    // Update window title
    SetWindowTextW(m_hwnd, Localization::T(Str::WindowTitle));
}

void MainWindow::OnNotify(LPARAM lParam) {
    LPNMHDR pnmhdr = (LPNMHDR)lParam;
    if (pnmhdr->idFrom == ID_TAB_CONTROL && pnmhdr->code == TCN_SELCHANGE) {
        int tabIndex = (int)SendMessage(m_hTabControl, TCM_GETCURSEL, 0, 0);
        SwitchTab(tabIndex);
    }
}

void MainWindow::OnLanguageChanged() {
    int langIndex = (int)SendMessageW(m_hLanguageCombo, CB_GETCURSEL, 0, 0);
    Language newLang = (langIndex == 0) ? Language::Vietnamese : Language::English;

    // Update language and refresh UI chrome + data
    Localization::SetLanguage(newLang);
    UpdateUITexts();               // Updates chrome (button, tabs, window title, language label)
                                    // and resets all content labels with new language prefixes
    RefreshLicenseData();           // Reload data from pipe with new language

    // Force the tab control and its child labels to repaint immediately. Without this,
    // the TCM_SETITEMW calls above (updating tab captions) can leave the tab body
    // showing stale/blank content until the next unrelated repaint.
    RedrawWindow(m_hTabControl, NULL, NULL,
        RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW | RDW_ERASE);
}

void MainWindow::SwitchTab(int tabIndex) {
    // Hide all Windows License labels
    ShowWindow(m_hWindowsNameLabel, SW_HIDE);
    ShowWindow(m_hWindowsEditionLabel, SW_HIDE);
    ShowWindow(m_hWindowsDescriptionLabel, SW_HIDE);
    ShowWindow(m_hWindowsLicenseDetailLabel, SW_HIDE);
    ShowWindow(m_hWindowsProductKeyLabel, SW_HIDE);
    ShowWindow(m_hWindowsKmsStatusLabel, SW_HIDE);
    ShowWindow(m_hWindowsKmsLabel, SW_HIDE);
    ShowWindow(m_hWindowsLastDetectedLabel, SW_HIDE);

    // Hide all Office License labels
    ShowWindow(m_hOfficeStatusLabel, SW_HIDE);
    ShowWindow(m_hOfficeLicenseNameLabel, SW_HIDE);
    ShowWindow(m_hOfficeLicenseStatusLabel, SW_HIDE);
    ShowWindow(m_hOfficeProductKeyLabel, SW_HIDE);
    ShowWindow(m_hOfficeKmsLabel, SW_HIDE);
    ShowWindow(m_hOfficeGracePeriodLabel, SW_HIDE);
    ShowWindow(m_hOfficeActivationIntervalLabel, SW_HIDE);

    // Hide all Settings labels and Language selector
    ShowWindow(m_hHostnameLabel, SW_HIDE);
    ShowWindow(m_hMachineGuidLabel, SW_HIDE);
    ShowWindow(m_hDepartmentLabel, SW_HIDE);
    ShowWindow(m_hServiceStatusLabel, SW_HIDE);
    ShowWindow(m_hLanguageLabel, SW_HIDE);
    ShowWindow(m_hLanguageCombo, SW_HIDE);

    // Show labels for the selected tab
    if (tabIndex == TAB_WINDOWS) {
        ShowWindow(m_hWindowsNameLabel, SW_SHOW);
        ShowWindow(m_hWindowsEditionLabel, SW_SHOW);
        ShowWindow(m_hWindowsDescriptionLabel, SW_SHOW);
        ShowWindow(m_hWindowsLicenseDetailLabel, SW_SHOW);
        ShowWindow(m_hWindowsProductKeyLabel, SW_SHOW);
        ShowWindow(m_hWindowsKmsStatusLabel, SW_SHOW);
        ShowWindow(m_hWindowsKmsLabel, SW_SHOW);
        ShowWindow(m_hWindowsLastDetectedLabel, SW_SHOW);
    } else if (tabIndex == TAB_OFFICE) {
        ShowWindow(m_hOfficeStatusLabel, SW_SHOW);
        ShowWindow(m_hOfficeLicenseNameLabel, SW_SHOW);
        ShowWindow(m_hOfficeLicenseStatusLabel, SW_SHOW);
        ShowWindow(m_hOfficeProductKeyLabel, SW_SHOW);
        ShowWindow(m_hOfficeKmsLabel, SW_SHOW);
        ShowWindow(m_hOfficeGracePeriodLabel, SW_SHOW);
        ShowWindow(m_hOfficeActivationIntervalLabel, SW_SHOW);
    } else if (tabIndex == TAB_SETTINGS) {
        ShowWindow(m_hHostnameLabel, SW_SHOW);
        ShowWindow(m_hMachineGuidLabel, SW_SHOW);
        ShowWindow(m_hDepartmentLabel, SW_SHOW);
        ShowWindow(m_hServiceStatusLabel, SW_SHOW);
        ShowWindow(m_hLanguageLabel, SW_SHOW);
        ShowWindow(m_hLanguageCombo, SW_SHOW);
        // Ensure Language label is updated with current language
        SetWindowTextW(m_hLanguageLabel, Localization::T(Str::LanguageLabel));
    }
}

void MainWindow::RefreshLicenseData() {
    if (!m_pipeClient) {
        SetWindowTextW(m_hRefreshLabel, Localization::T(Str::ServiceNotConnected));
        return;
    }

    std::string response = m_pipeClient->SendCommand("GetLicenseData");
    if (response.empty()) {
        SetWindowTextW(m_hRefreshLabel, Localization::T(Str::FailedToGetData));
        return;
    }

    try {
        auto json = nlohmann::json::parse(response);

        if (json["status"] == "success" && json.contains("data")) {
            auto data = json["data"];

            // Windows License
            if (data.contains("windows")) {
                auto win = data["windows"];
                int licenseStatus = win.value("licenseStatus", 0);
                int kmsStatus = win.value("kmsStatus", 0);

                // Windows Name
                std::wstring nameStr = std::wstring(Localization::T(Str::Name)) + L" " +
                    Localization::Widen(win.value("name", std::string("Unknown")));
                SetWindowTextW(m_hWindowsNameLabel, nameStr.c_str());

                // Windows Edition
                std::wstring edition = std::wstring(Localization::T(Str::Edition)) + L" " +
                    Localization::Widen(win.value("edition", std::string("Unknown")));
                SetWindowTextW(m_hWindowsEditionLabel, edition.c_str());

                // Windows Description
                std::wstring descStr = std::wstring(Localization::T(Str::Description)) + L" " +
                    Localization::Widen(win.value("description", std::string("Unknown")));
                SetWindowTextW(m_hWindowsDescriptionLabel, descStr.c_str());

                // License Status Detail (with translation of status value)
                std::wstring licenseDetail = std::wstring(Localization::T(Str::LicenseStatus)) + L" " +
                    Localization::TranslateStatusValue(win.value("licenseStatusDetail", std::string("Unknown")));
                SetWindowTextW(m_hWindowsLicenseDetailLabel, licenseDetail.c_str());

                // Product Key
                std::wstring keyStr = std::wstring(Localization::T(Str::ProductKey)) + L" " +
                    Localization::Widen(win.value("partialProductKey", std::string("Unknown")));
                SetWindowTextW(m_hWindowsProductKeyLabel, keyStr.c_str());

                // KMS Status
                std::wstring kmsStatusStr = Localization::T(Str::KmsStatus);
                kmsStatusStr += L" ";
                switch (kmsStatus) {
                    case 0: kmsStatusStr += Localization::T(Str::KmsNotUsing); break;
                    case 1: kmsStatusStr += Localization::T(Str::KmsDetected); break;
                    case 2: kmsStatusStr += Localization::T(Str::KmsNotFound); break;
                    case 3: kmsStatusStr += Localization::T(Str::KmsError); break;
                    default: kmsStatusStr += Localization::T(Str::Unknown); break;
                }
                SetWindowTextW(m_hWindowsKmsStatusLabel, kmsStatusStr.c_str());

                // KMS Server
                std::wstring kmsServer = std::wstring(Localization::T(Str::KmsServer)) + L" " +
                    Localization::Widen(win.value("kmsServer", std::string("Not Detected")));
                SetWindowTextW(m_hWindowsKmsLabel, kmsServer.c_str());

                // Last Detected
                std::wstring lastDetected = std::wstring(Localization::T(Str::LastDetected)) + L" " +
                    Localization::Widen(win.value("lastDetected", std::string("Never")));
                SetWindowTextW(m_hWindowsLastDetectedLabel, lastDetected.c_str());
            }

            // Office License
            if (data.contains("office")) {
                auto office = data["office"];
                std::string officeLicenseStatus = office.value("licenseStatus", std::string("Not Detected"));
                std::string licenseName = office.value("licenseName", std::string("Not Detected"));

                // Status
                std::wstring statusDisplay = std::wstring(Localization::T(Str::Status)) + L" " +
                    (officeLicenseStatus.empty() ? Localization::T(Str::NotDetected) :
                     Localization::TranslateStatusValue(officeLicenseStatus));
                SetWindowTextW(m_hOfficeStatusLabel, statusDisplay.c_str());

                // License Name
                std::wstring licNameStr = std::wstring(Localization::T(Str::LicenseName)) + L" " +
                    Localization::Widen(licenseName);
                SetWindowTextW(m_hOfficeLicenseNameLabel, licNameStr.c_str());

                // License Status Detail
                std::wstring licStatusDetail = std::wstring(Localization::T(Str::LicenseStatus)) + L" " +
                    Localization::TranslateStatusValue(officeLicenseStatus);
                SetWindowTextW(m_hOfficeLicenseStatusLabel, licStatusDetail.c_str());

                // Product Key
                std::wstring officeKey = Localization::Widen(office.value("partialProductKey", std::string("Unknown")));
                std::wstring officeKeyStr = std::wstring(Localization::T(Str::ProductKey)) + L" " + officeKey;
                SetWindowTextW(m_hOfficeProductKeyLabel, officeKeyStr.c_str());

                // KMS Server
                std::wstring officeKmsServer = Localization::Widen(office.value("kmsServer", std::string("Not Detected")));
                std::wstring kmsLabel = std::wstring(Localization::T(Str::KmsServer)) + L" " + officeKmsServer;
                SetWindowTextW(m_hOfficeKmsLabel, kmsLabel.c_str());

                // Grace Period
                std::wstring remainingGrace = Localization::Widen(office.value("remainingGrace", std::string("Unknown")));
                std::wstring graceStr = std::wstring(Localization::T(Str::GracePeriod)) + L" " + remainingGrace;
                SetWindowTextW(m_hOfficeGracePeriodLabel, graceStr.c_str());

                // Activation Interval
                std::wstring activationInterval = Localization::Widen(office.value("activationInterval", std::string("Unknown")));
                std::wstring activationStr = std::wstring(Localization::T(Str::ActivationInterval)) + L" " + activationInterval;
                SetWindowTextW(m_hOfficeActivationIntervalLabel, activationStr.c_str());
            } else {
                SetWindowTextW(m_hOfficeStatusLabel,
                    (std::wstring(Localization::T(Str::Status)) + L" " +
                     Localization::T(Str::NotDetected)).c_str());
                SetWindowTextW(m_hOfficeLicenseNameLabel,
                    (std::wstring(Localization::T(Str::LicenseName)) + L" " +
                     Localization::T(Str::NotDetected)).c_str());
                SetWindowTextW(m_hOfficeLicenseStatusLabel,
                    (std::wstring(Localization::T(Str::LicenseStatus)) + L" " +
                     Localization::T(Str::NotDetected)).c_str());
                SetWindowTextW(m_hOfficeProductKeyLabel,
                    (std::wstring(Localization::T(Str::ProductKey)) + L" " +
                     Localization::T(Str::NotDetected)).c_str());
                SetWindowTextW(m_hOfficeKmsLabel,
                    (std::wstring(Localization::T(Str::KmsServer)) + L" " +
                     Localization::T(Str::NotDetected)).c_str());
                SetWindowTextW(m_hOfficeGracePeriodLabel,
                    (std::wstring(Localization::T(Str::GracePeriod)) + L" " +
                     Localization::T(Str::NotDetected)).c_str());
                SetWindowTextW(m_hOfficeActivationIntervalLabel,
                    (std::wstring(Localization::T(Str::ActivationInterval)) + L" " +
                     Localization::T(Str::NotDetected)).c_str());
            }

            // Settings - get hostname from system
            wchar_t computerName[MAX_COMPUTERNAME_LENGTH + 1];
            DWORD computerNameLen = MAX_COMPUTERNAME_LENGTH + 1;
            if (GetComputerNameW(computerName, &computerNameLen)) {
                std::wstring hostnameStr = std::wstring(Localization::T(Str::Hostname)) + L" " + computerName;
                SetWindowTextW(m_hHostnameLabel, hostnameStr.c_str());
            } else {
                SetWindowTextW(m_hHostnameLabel,
                    (std::wstring(Localization::T(Str::Hostname)) + L" " +
                     Localization::T(Str::Unknown)).c_str());
            }

            // Machine GUID - try to get from registry
            HKEY hKey;
            if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
                wchar_t guidBuf[256];
                DWORD size = sizeof(guidBuf);
                if (RegQueryValueExW(hKey, L"MachineGuid", NULL, NULL, (LPBYTE)guidBuf, &size) == ERROR_SUCCESS) {
                    std::wstring guidStr = std::wstring(Localization::T(Str::MachineGuid)) + L" " + guidBuf;
                    SetWindowTextW(m_hMachineGuidLabel, guidStr.c_str());
                } else {
                    SetWindowTextW(m_hMachineGuidLabel,
                        (std::wstring(Localization::T(Str::MachineGuid)) + L" " +
                         Localization::T(Str::Unknown)).c_str());
                }
                RegCloseKey(hKey);
            } else {
                SetWindowTextW(m_hMachineGuidLabel,
                    (std::wstring(Localization::T(Str::MachineGuid)) + L" " +
                     Localization::T(Str::Unknown)).c_str());
            }

            // Department - placeholder (from pipe data if available, otherwise Unknown)
            SetWindowTextW(m_hDepartmentLabel,
                (std::wstring(Localization::T(Str::Department)) + L" " +
                 Localization::T(Str::Unknown)).c_str());

            SetWindowTextW(m_hServiceStatusLabel,
                (std::wstring(Localization::T(Str::ServiceStatus)) + L" " +
                 Localization::T(Str::Online)).c_str());
            SetWindowTextW(m_hRefreshLabel, Localization::T(Str::LastUpdatedNow));
        }
    } catch (const std::exception& ex) {
        SetWindowTextW(m_hRefreshLabel, Localization::T(Str::ParseError));
    }
}

void MainWindow::CheckNow() {
    SetWindowTextW(m_hRefreshLabel, Localization::T(Str::Checking));
    if (!m_pipeClient) {
        SetWindowTextW(m_hRefreshLabel, Localization::T(Str::ServiceNotConnected));
        return;
    }

    try {
        std::string response = m_pipeClient->SendCommand("RequestImmediateCheck");
        if (response.empty()) {
            SetWindowTextW(m_hRefreshLabel, Localization::T(Str::FailedToQueueCheck));
            return;
        }

        Sleep(3000);  // Wait for detection to complete
        RefreshLicenseData();
    }
    catch (const std::exception& ex) {
        SetWindowTextW(m_hRefreshLabel, Localization::T(Str::ErrorDuringCheck));
    }
    catch (...) {
        SetWindowTextW(m_hRefreshLabel, Localization::T(Str::UnknownErrorDuringCheck));
    }
}
