#include "MainWindow.h"
#include "Localization.h"
#include "DetectionWorker.h"
#include "resource.h"
#include <string>
#include <algorithm>
#include <commctrl.h>
#include <shlobj.h>
#include <windowsx.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <winreg.h>
#include <objidl.h>
#include <gdiplus.h>
#include <dwmapi.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "dwmapi.lib")

// Missing from the Windows 10.0.19041 SDK's dwmapi.h (added in later SDKs) -
// values are stable/public, safe to declare ourselves. DwmSetWindowAttribute
// silently no-ops on Windows versions that don't support them.
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif

extern HINSTANCE g_hInstance;

// Posted by DetectionWorker's callback (worker thread) to marshal a finished
// LicenseResult back to the UI thread. lParam owns a heap-allocated
// LicenseResult*, freed by the handler in WndProc.
const UINT WM_APP_LICENSE_READY = WM_APP + 1;

namespace {

// -- Modern theme palette -----------------------------------------------
const COLORREF kWindowBackground = RGB(246, 247, 249);
const COLORREF kTextColor = RGB(40, 44, 49);

enum class BadgeStatus { Valid, Cracked, NotActivated, Unknown };

// Windows: LicenseStatus's raw ordinal - 0=Legitimate, 1=Cracked,
// 2=NotLicensed, 3=UnableToDetermine (LicenseStatusEnum.h).
BadgeStatus ClassifyWindowsStatus(int licenseStatusCode) {
    switch (licenseStatusCode) {
        case 0:  return BadgeStatus::Valid;
        case 1:  return BadgeStatus::Cracked;
        case 2:  return BadgeStatus::NotActivated;
        default: return BadgeStatus::Unknown;
    }
}

// Office has no enum on the wire, just OSPP's raw "---XXX---" token (see
// OfficeOSPPDetector.cpp). Mirrors the heuristic used for server reporting
// in ServerReporter.cpp's OfficeStatusToRealString().
BadgeStatus ClassifyOfficeStatus(const std::string& rawStatus) {
    if (rawStatus.find("NON_GENUINE") != std::string::npos) {
        return BadgeStatus::Cracked;
    }
    if (rawStatus.find("LICENSED") != std::string::npos) {
        return BadgeStatus::Valid;
    }
    if (rawStatus.find("GRACE") != std::string::npos ||
        rawStatus.find("NOTIFICATIONS") != std::string::npos ||
        rawStatus.find("HOLD") != std::string::npos) {
        return BadgeStatus::NotActivated;
    }
    return BadgeStatus::Unknown;
}

Gdiplus::Color BadgeColor(BadgeStatus status) {
    switch (status) {
        case BadgeStatus::Valid:        return Gdiplus::Color(255, 30, 130, 76);
        case BadgeStatus::Cracked:      return Gdiplus::Color(255, 197, 48, 48);
        case BadgeStatus::NotActivated: return Gdiplus::Color(255, 191, 128, 22);
        default:                        return Gdiplus::Color(255, 120, 126, 134);
    }
}

void AddRoundedRect(Gdiplus::GraphicsPath& path, const Gdiplus::RectF& rect, float radius) {
    float d = radius * 2.0f;
    path.AddArc(rect.X, rect.Y, d, d, 180, 90);
    path.AddArc(rect.X + rect.Width - d, rect.Y, d, d, 270, 90);
    path.AddArc(rect.X + rect.Width - d, rect.Y + rect.Height - d, d, d, 0, 90);
    path.AddArc(rect.X, rect.Y + rect.Height - d, d, d, 90, 90);
    path.CloseFigure();
}

// Shared by WndProc (m_hwnd's own STATIC children) and TabProc (labels
// nested under m_hTabControl, which is their real WM_CTLCOLORSTATIC target).
// Callers pass the brush matching whatever's actually behind that label
// (the tab body vs. m_hwnd's own background) so it blends in instead of
// showing as a mismatched box.
//
// Must return a real (non-hollow) background brush: the static control's
// default paint handler fills its rectangle with whatever brush is returned
// here before drawing the text. NULL_BRUSH skips that fill entirely, so on
// every SetWindowText() the new text gets drawn over the old one instead of
// replacing it.
LRESULT HandleCtlColorStatic(WPARAM wParam, HBRUSH backgroundBrush) {
    HDC hdc = (HDC)wParam;
    SetTextColor(hdc, kTextColor);
    SetBkMode(hdc, TRANSPARENT);
    return (LRESULT)backgroundBrush;
}

bool IsSystemDarkModeEnabled() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return false;
    }
    DWORD value = 1;  // Default to light if the value is missing.
    DWORD size = sizeof(value);
    RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr, (LPBYTE)&value, &size);
    RegCloseKey(hKey);
    return value == 0;
}

} // namespace

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

// Bottom-row layout (refresh status label + Check Now button). Shared by
// CreateControls() (initial placement) and MainWindow::OnSize() (kept in
// sync with the window's actual client size) so the two controls are
// always computed from the same margins and never overlap.
const int kCheckButtonWidth = 160;
const int kCheckButtonHeight = 34;
const int kBottomMargin = 20;
const int kRefreshLabelHeight = 20;

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
      m_windowsLicenseStatusCode(3), m_origTabWndProc(nullptr) {
}

MainWindow::~MainWindow() {
    // Stop the worker (joins its thread) before the window/handles it might
    // still post messages to go away.
    if (m_detectionWorker) {
        m_detectionWorker->Stop();
    }
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
    wc.hbrBackground = CreateSolidBrush(kWindowBackground);
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

    ApplyDwmVisuals();
    CreateControls();

    // Runs detection in-process (no more separate Agent process/service):
    // checks immediately, then every 5 minutes while this window is open.
    // The callback runs on the worker thread, so it only marshals the
    // result to the UI thread via PostMessage - actual rendering happens
    // in OnLicenseResultReady() on receipt of WM_APP_LICENSE_READY.
    m_detectionWorker = std::make_unique<DetectionWorker>(std::chrono::seconds(300),
        [this](const LicenseResult& result) {
            LicenseResult* copy = new LicenseResult(result);
            if (!PostMessageW(m_hwnd, WM_APP_LICENSE_READY, 0, (LPARAM)copy)) {
                delete copy;
            }
        });
    m_detectionWorker->Start();

    return true;
}

void MainWindow::Show(int nCmdShow) {
    if (m_hwnd) {
        ShowWindow(m_hwnd, nCmdShow);
        UpdateWindow(m_hwnd);
    }
}

// Dark title bar (matches the system theme) and rounded window corners
// (Windows 11 only). Both attributes are unknown to the 10.0.19041 SDK this
// project builds against - DwmSetWindowAttribute simply returns an error
// and does nothing on Windows versions/builds that don't support them, so
// this is safe to call unconditionally.
void MainWindow::ApplyDwmVisuals() {
    BOOL useDarkMode = IsSystemDarkModeEnabled() ? TRUE : FALSE;
    DwmSetWindowAttribute(m_hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));

    DWORD cornerPreference = DWMWCP_ROUND;
    DwmSetWindowAttribute(m_hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPreference, sizeof(cornerPreference));
}

// Paints one of the two owner-drawn "License Status: <value>" labels: the
// prefix in plain text, then the value inside a colored rounded pill (green/
// red/amber/gray) so the status reads at a glance instead of being just
// another line of text.
void MainWindow::DrawStatusLabel(DRAWITEMSTRUCT* dis) {
    wchar_t buf[512] = {0};
    GetWindowTextW(dis->hwndItem, buf, _countof(buf));
    std::wstring text(buf);

    BadgeStatus status = (dis->hwndItem == m_hWindowsLicenseDetailLabel)
        ? ClassifyWindowsStatus(m_windowsLicenseStatusCode)
        : ClassifyOfficeStatus(m_officeLicenseStatusRaw);

    Gdiplus::Graphics graphics(dis->hDC);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

    // The item isn't pre-erased for SS_OWNERDRAW statics - fill it to match
    // the (non-owner-drawn) tab body around it so there's no visible seam.
    COLORREF btnFace = GetSysColor(COLOR_BTNFACE);
    Gdiplus::SolidBrush bgBrush(Gdiplus::Color(255,
        GetRValue(btnFace), GetGValue(btnFace), GetBValue(btnFace)));
    graphics.FillRectangle(&bgBrush, (Gdiplus::REAL)dis->rcItem.left, (Gdiplus::REAL)dis->rcItem.top,
        (Gdiplus::REAL)(dis->rcItem.right - dis->rcItem.left),
        (Gdiplus::REAL)(dis->rcItem.bottom - dis->rcItem.top));

    size_t sep = text.find(L": ");
    std::wstring prefix = (sep == std::wstring::npos) ? text : text.substr(0, sep + 1);
    std::wstring value = (sep == std::wstring::npos) ? L"" : text.substr(sep + 2);

    // 12pt matches the other labels' CreateFontW(-16, ...) (-16 device px at
    // 96 DPI == 12pt), so the badge line reads the same size as its siblings.
    Gdiplus::FontFamily fontFamily(L"Segoe UI");
    Gdiplus::Font font(&fontFamily, 12.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
    Gdiplus::SolidBrush textBrush(Gdiplus::Color(255,
        GetRValue(kTextColor), GetGValue(kTextColor), GetBValue(kTextColor)));

    Gdiplus::REAL originX = (Gdiplus::REAL)dis->rcItem.left;
    Gdiplus::REAL originY = (Gdiplus::REAL)dis->rcItem.top + 2.0f;
    graphics.DrawString(prefix.c_str(), -1, &font, Gdiplus::PointF(originX, originY), &textBrush);

    Gdiplus::RectF prefixBounds;
    graphics.MeasureString(prefix.c_str(), -1, &font, Gdiplus::PointF(0, 0), &prefixBounds);

    if (!value.empty()) {
        Gdiplus::RectF valueBounds;
        graphics.MeasureString(value.c_str(), -1, &font, Gdiplus::PointF(0, 0), &valueBounds);

        const float padX = 10.0f, padY = 3.0f;
        Gdiplus::RectF pillRect(
            originX + prefixBounds.Width + 6.0f,
            originY - 2.0f,
            valueBounds.Width + padX * 2.0f,
            valueBounds.Height + padY * 2.0f - 4.0f);

        Gdiplus::GraphicsPath path;
        AddRoundedRect(path, pillRect, pillRect.Height / 2.0f);
        Gdiplus::SolidBrush pillBrush(BadgeColor(status));
        graphics.FillPath(&pillBrush, &path);

        Gdiplus::SolidBrush pillTextBrush(Gdiplus::Color(255, 255, 255, 255));
        graphics.DrawString(value.c_str(), -1, &font,
            Gdiplus::PointF(pillRect.X + padX, pillRect.Y + padY - 2.0f), &pillTextBrush);
    }
}

LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Can arrive before WM_CREATE (while the window is still being sized),
    // so handle it before the GWLP_USERDATA/pThis lookup below - it needs no
    // per-instance state. Without a floor, shrinking the window below what
    // the fixed-width bottom-row controls need clips their text (e.g. the
    // refresh label) instead of reflowing it.
    if (msg == WM_GETMINMAXINFO) {
        MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
        mmi->ptMinTrackSize.x = 800;
        mmi->ptMinTrackSize.y = 400;
        return 0;
    }

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

    case WM_APP_LICENSE_READY: {
        LicenseResult* result = reinterpret_cast<LicenseResult*>(lParam);
        if (result) {
            pThis->OnLicenseResultReady(*result);
            delete result;
        }
        return 0;
    }

    case WM_CTLCOLORSTATIC:
        // Same brush as the window class background (set in Create()) so
        // m_hwnd's own STATIC children (e.g. m_hRefreshLabel) blend in
        // instead of showing a mismatched box.
        return HandleCtlColorStatic(wParam, (HBRUSH)GetClassLongPtr(hwnd, GCLP_HBRBACKGROUND));

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

// Subclass of m_hTabControl (SysTabControl32). All of the Windows/Office/
// Settings content labels are created as children of the tab control, not
// m_hwnd, so WM_CTLCOLORSTATIC and WM_DRAWITEM for them land here rather
// than in MainWindow::WndProc.
LRESULT CALLBACK MainWindow::TabProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MainWindow* pThis = reinterpret_cast<MainWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    if (pThis) {
        if (msg == WM_CTLCOLORSTATIC) {
            return HandleCtlColorStatic(wParam, GetSysColorBrush(COLOR_BTNFACE));
        }
        if (msg == WM_DRAWITEM) {
            DRAWITEMSTRUCT* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            if (dis->hwndItem == pThis->m_hWindowsLicenseDetailLabel ||
                dis->hwndItem == pThis->m_hOfficeLicenseStatusLabel) {
                pThis->DrawStatusLabel(dis);
                return TRUE;
            }
        }
    }

    if (pThis && pThis->m_origTabWndProc) {
        return CallWindowProc(pThis->m_origTabWndProc, hwnd, msg, wParam, lParam);
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void MainWindow::CreateControls() {
    // Tab Control
    m_hTabControl = CreateWindowW(
        L"SysTabControl32", L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | TCS_TABS,
        10, 10, 780, 280,
        m_hwnd, (HMENU)ID_TAB_CONTROL, g_hInstance, NULL
    );

    // Subclass the tab control so its child labels' WM_CTLCOLORSTATIC/
    // WM_DRAWITEM (their real parent) land in TabProc.
    SetWindowLongPtr(m_hTabControl, GWLP_USERDATA, (LONG_PTR)this);
    m_origTabWndProc = (WNDPROC)SetWindowLongPtr(m_hTabControl, GWLP_WNDPROC, (LONG_PTR)TabProc);

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
    int lineHeight = 26;

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
        WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
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
        WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
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

    // Language selector in Settings tab (created as child of main window, not tab control,
    // so its WM_COMMAND lands on m_hwnd's WndProc). Because of that, its position must be
    // converted from "coordinates relative to m_hTabControl" (like every other Settings row)
    // into m_hwnd-relative coordinates. A tab control's children are positioned from its
    // client-area origin (0,0) same as any other parent - NOT offset by the tab strip (every
    // other row already accounts for that by starting at y=50) - so the conversion is just a
    // straight translation by the tab control's own position within m_hwnd.
    m_hLanguageLabel = CreateWindowW(
        L"STATIC", L"Language:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, settingsYPos, 100, lineHeight,
        m_hTabControl, NULL, g_hInstance, NULL
    );

    const int tabControlX = 10, tabControlY = 10;  // matches m_hTabControl's CreateWindowW position
    int comboX = tabControlX + 140;
    int comboY = tabControlY + settingsYPos - 2;  // -2: nudge up to visually center against the label

    m_hLanguageCombo = CreateWindowW(
        L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
        comboX, comboY, 120, 200,
        m_hwnd, (HMENU)ID_LANGUAGE_COMBO, g_hInstance, NULL
    );

    SendMessageW(m_hLanguageCombo, CB_ADDSTRING, 0, (LPARAM)L"Tiếng Việt");
    SendMessageW(m_hLanguageCombo, CB_ADDSTRING, 0, (LPARAM)L"English");
    SendMessageW(m_hLanguageCombo, CB_SETCURSEL, 0, 0);  // Default to Vietnamese

    // Bottom buttons (outside tabs) - sized/positioned properly by
    // LayoutBottomControls() below, called once the window's real client
    // size is known; these initial values are just placeholders.
    m_hRefreshLabel = CreateWindowW(
        L"STATIC", L"Ready",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        kBottomMargin, 300, 100, kRefreshLabelHeight,
        m_hwnd, NULL, g_hInstance, NULL
    );

    m_hCheckButton = CreateWindowW(
        L"BUTTON", L"Check Now",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        620, 292, kCheckButtonWidth, kCheckButtonHeight,
        m_hwnd, (HMENU)ID_CHECK_BUTTON, g_hInstance, NULL
    );

    // Set font - Segoe UI has shipped on every Windows since Vista; falls
    // back to the stock GUI font if somehow unavailable.
    HFONT hFont = CreateFontW(
        -16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    if (!hFont) {
        hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    }
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

    // Position the bottom row using the window's real client size (which is
    // smaller than the "800x400" passed to CreateWindowExW - that includes
    // the title bar/borders) rather than the placeholder values used above.
    RECT clientRect;
    GetClientRect(m_hwnd, &clientRect);
    LayoutBottomControls(clientRect.right, clientRect.bottom);
}

// Keeps the refresh label and Check Now button from ever overlapping: the
// label's width is computed from whatever space is actually left after the
// fixed-size button, instead of both using independent hardcoded widths.
void MainWindow::LayoutBottomControls(int cx, int cy) {
    if (m_hCheckButton) {
        MoveWindow(m_hCheckButton, cx - kCheckButtonWidth - kBottomMargin, cy - kCheckButtonHeight - kBottomMargin,
            kCheckButtonWidth, kCheckButtonHeight, TRUE);
    }
    if (m_hRefreshLabel) {
        int labelWidth = cx - kCheckButtonWidth - kBottomMargin * 3;
        if (labelWidth < 100) {
            labelWidth = 100;
        }
        int labelY = cy - kBottomMargin - (kCheckButtonHeight + kRefreshLabelHeight) / 2;
        MoveWindow(m_hRefreshLabel, kBottomMargin, labelY, labelWidth, kRefreshLabelHeight, TRUE);
    }
}

void MainWindow::OnSize(int cx, int cy) {
    LayoutBottomControls(cx, cy);
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
    OnLicenseResultReady(m_lastResult);  // Re-render the last known result in the new language

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

void MainWindow::OnLicenseResultReady(const LicenseResult& result) {
    m_lastResult = result;

    const WindowsLicenseInfo& winInfo = result.GetWindowsLicenseInfo();
    const OfficeLicenseInfo& officeInfo = result.GetOfficeLicenseInfo();

    m_windowsLicenseStatusCode = static_cast<int>(result.GetLicenseStatus());
    m_officeLicenseStatusRaw = officeInfo.licenseStatus;

    // Windows Name
    SetWindowTextW(m_hWindowsNameLabel,
        (std::wstring(Localization::T(Str::Name)) + L" " + Localization::Widen(winInfo.name)).c_str());

    // Windows Edition
    SetWindowTextW(m_hWindowsEditionLabel,
        (std::wstring(Localization::T(Str::Edition)) + L" " + Localization::Widen(result.GetWindowsEdition())).c_str());

    // Windows Description
    SetWindowTextW(m_hWindowsDescriptionLabel,
        (std::wstring(Localization::T(Str::Description)) + L" " + Localization::Widen(winInfo.description)).c_str());

    // License Status Detail (with translation of status value)
    SetWindowTextW(m_hWindowsLicenseDetailLabel,
        (std::wstring(Localization::T(Str::LicenseStatus)) + L" " +
         Localization::TranslateStatusValue(winInfo.licenseStatus)).c_str());

    // Product Key
    SetWindowTextW(m_hWindowsProductKeyLabel,
        (std::wstring(Localization::T(Str::ProductKey)) + L" " + Localization::Widen(winInfo.partialProductKey)).c_str());

    // KMS Status
    std::wstring kmsStatusStr = Localization::T(Str::KmsStatus);
    kmsStatusStr += L" ";
    switch (result.GetKmsStatus()) {
        case KMSStatus::NotKMS:      kmsStatusStr += Localization::T(Str::KmsNotUsing); break;
        case KMSStatus::KMSDetected: kmsStatusStr += Localization::T(Str::KmsDetected); break;
        case KMSStatus::KMSNotFound: kmsStatusStr += Localization::T(Str::KmsNotFound); break;
        case KMSStatus::Error:       kmsStatusStr += Localization::T(Str::KmsError); break;
        default:                     kmsStatusStr += Localization::T(Str::Unknown); break;
    }
    SetWindowTextW(m_hWindowsKmsStatusLabel, kmsStatusStr.c_str());

    // KMS Server
    SetWindowTextW(m_hWindowsKmsLabel,
        (std::wstring(Localization::T(Str::KmsServer)) + L" " + Localization::Widen(result.GetWindowsKmsServer())).c_str());

    // Last Detected
    {
        std::time_t t = std::chrono::system_clock::to_time_t(result.GetTimestamp());
        struct tm timeinfo;
        localtime_s(&timeinfo, &t);
        wchar_t buf[32] = {0};
        wcsftime(buf, _countof(buf), L"%d/%m/%Y %H:%M:%S", &timeinfo);
        SetWindowTextW(m_hWindowsLastDetectedLabel,
            (std::wstring(Localization::T(Str::LastDetected)) + L" " + buf).c_str());
    }

    // Office: Status
    SetWindowTextW(m_hOfficeStatusLabel,
        (std::wstring(Localization::T(Str::Status)) + L" " +
         (officeInfo.licenseStatus.empty() ? Localization::T(Str::NotDetected) :
          Localization::TranslateStatusValue(officeInfo.licenseStatus))).c_str());

    // Office: License Name
    SetWindowTextW(m_hOfficeLicenseNameLabel,
        (std::wstring(Localization::T(Str::LicenseName)) + L" " + Localization::Widen(officeInfo.licenseName)).c_str());

    // Office: License Status Detail
    SetWindowTextW(m_hOfficeLicenseStatusLabel,
        (std::wstring(Localization::T(Str::LicenseStatus)) + L" " +
         Localization::TranslateStatusValue(officeInfo.licenseStatus)).c_str());

    // Office: Product Key
    SetWindowTextW(m_hOfficeProductKeyLabel,
        (std::wstring(Localization::T(Str::ProductKey)) + L" " + Localization::Widen(officeInfo.partialProductKey)).c_str());

    // Office: KMS Server (top-level accessor, not officeInfo.kmsServer - matches what was reported before)
    SetWindowTextW(m_hOfficeKmsLabel,
        (std::wstring(Localization::T(Str::KmsServer)) + L" " + Localization::Widen(result.GetOfficeKmsServer())).c_str());

    // Office: Grace Period
    SetWindowTextW(m_hOfficeGracePeriodLabel,
        (std::wstring(Localization::T(Str::GracePeriod)) + L" " + Localization::Widen(officeInfo.remainingGrace)).c_str());

    // Office: Activation Interval
    SetWindowTextW(m_hOfficeActivationIntervalLabel,
        (std::wstring(Localization::T(Str::ActivationInterval)) + L" " + Localization::Widen(officeInfo.activationInterval)).c_str());

    // Settings - hostname from system
    wchar_t computerName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD computerNameLen = MAX_COMPUTERNAME_LENGTH + 1;
    if (GetComputerNameW(computerName, &computerNameLen)) {
        SetWindowTextW(m_hHostnameLabel,
            (std::wstring(Localization::T(Str::Hostname)) + L" " + computerName).c_str());
    } else {
        SetWindowTextW(m_hHostnameLabel,
            (std::wstring(Localization::T(Str::Hostname)) + L" " + Localization::T(Str::Unknown)).c_str());
    }

    // Machine GUID - from registry
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t guidBuf[256];
        DWORD size = sizeof(guidBuf);
        if (RegQueryValueExW(hKey, L"MachineGuid", NULL, NULL, (LPBYTE)guidBuf, &size) == ERROR_SUCCESS) {
            SetWindowTextW(m_hMachineGuidLabel,
                (std::wstring(Localization::T(Str::MachineGuid)) + L" " + guidBuf).c_str());
        } else {
            SetWindowTextW(m_hMachineGuidLabel,
                (std::wstring(Localization::T(Str::MachineGuid)) + L" " + Localization::T(Str::Unknown)).c_str());
        }
        RegCloseKey(hKey);
    } else {
        SetWindowTextW(m_hMachineGuidLabel,
            (std::wstring(Localization::T(Str::MachineGuid)) + L" " + Localization::T(Str::Unknown)).c_str());
    }

    // Department - placeholder (no source of truth for this yet)
    SetWindowTextW(m_hDepartmentLabel,
        (std::wstring(Localization::T(Str::Department)) + L" " + Localization::T(Str::Unknown)).c_str());

    SetWindowTextW(m_hServiceStatusLabel,
        (std::wstring(Localization::T(Str::ServiceStatus)) + L" " +
         Localization::T(result.IsError() ? Str::Offline : Str::Online)).c_str());

    SetWindowTextW(m_hRefreshLabel,
        Localization::T(result.IsError() ? Str::ErrorDuringCheck : Str::LastUpdatedNow));
}

void MainWindow::CheckNow() {
    SetWindowTextW(m_hRefreshLabel, Localization::T(Str::Checking));
    if (m_detectionWorker) {
        m_detectionWorker->RequestImmediateCheck();
    }
}
